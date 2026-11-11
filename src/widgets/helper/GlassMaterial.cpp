// SPDX-License-Identifier: MIT
#include "widgets/helper/GlassMaterial.hpp"

#include "common/UiPalette.hpp"
#include "util/NativeGlass.hpp"

#include <QApplication>
#include <QEvent>
#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>

namespace chatterino {
namespace {
QHash<QWidget *, QPointer<GlassBackdrop>> &samplers()
{
    static QHash<QWidget *, QPointer<GlassBackdrop>> values;
    return values;
}
thread_local int captureDepth = 0;
QPointer<QWidget> activeSource;

float mix(float a, float b, float amount)
{
    return a + (b - a) * amount;
}

QRgb sample(const QImage &image, float x, float y)
{
    x = std::clamp(x, 0.0F, static_cast<float>(image.width() - 1));
    y = std::clamp(y, 0.0F, static_cast<float>(image.height() - 1));
    const int ix = static_cast<int>(x);
    const int iy = static_cast<int>(y);
    const int jx = std::min(ix + 1, image.width() - 1);
    const int jy = std::min(iy + 1, image.height() - 1);
    const auto *a = reinterpret_cast<const QRgb *>(image.constScanLine(iy));
    const auto *b = reinterpret_cast<const QRgb *>(image.constScanLine(jy));
    const auto channel = [&](auto get) {
        return qRound(mix(mix(get(a[ix]), get(a[jx]), x - ix),
                          mix(get(b[ix]), get(b[jx]), x - ix), y - iy));
    };
    return qRgb(channel([](QRgb c) {
                    return qRed(c);
                }),
                channel([](QRgb c) {
                    return qGreen(c);
                }),
                channel([](QRgb c) {
                    return qBlue(c);
                }));
}

bool sameOptions(const GlassOptions &a, const GlassOptions &b)
{
    return a.role == b.role && a.edge == b.edge && a.radius == b.radius &&
           a.light == b.light && a.active == b.active &&
           a.nativeBackdrop == b.nativeBackdrop && a.refraction == b.refraction;
}
}  // namespace

GlassBackdrop::GlassBackdrop(QWidget *source)
    : QObject(source)
    , source_(source)
{
    this->timer_.setSingleShot(true);
    this->timer_.setInterval(250);
    connect(&this->timer_, &QTimer::timeout, this, [this] {
        this->refresh();
    });
    source->installEventFilter(this);
}

GlassBackdrop *GlassBackdrop::watch(QWidget *chatView)
{
    if (!chatView)
    {
        return nullptr;
    }
    if (const auto existing = samplers().value(chatView))
    {
        return existing;
    }
    auto *sampler = new GlassBackdrop(chatView);
    samplers().insert(chatView, sampler);
    if (!activeSource || chatView->hasFocus())
    {
        activeSource = chatView;
    }
    connect(chatView, &QObject::destroyed, sampler, [chatView] {
        samplers().remove(chatView);
    });
    return sampler;
}

GlassBackdrop *GlassBackdrop::forWidget(QWidget *consumer)
{
    if (!consumer)
    {
        return nullptr;
    }
    auto *root = consumer->window();
    if (root->parentWidget())
    {
        root = root->parentWidget()->window();
    }
    if (activeSource && activeSource->isVisible() &&
        activeSource->window() == root)
    {
        return samplers().value(activeSource);
    }
    for (auto it = samplers().cbegin(); it != samplers().cend(); ++it)
    {
        if (it.value() && it.key()->isVisible() && it.key()->window() == root)
        {
            return it.value();
        }
    }
    // Menus without a QWidget owner can use the last focused chat view. The
    // registry contains chat views exclusively, never login/settings widgets.
    return activeSource && activeSource->isVisible()
               ? samplers().value(activeSource)
               : nullptr;
}

bool GlassBackdrop::isCapturing()
{
    return captureDepth != 0;
}

void GlassBackdrop::addConsumer(QWidget *consumer, const QRect &region)
{
    std::erase_if(this->consumers_, [](const auto &p) {
        return p.widget.isNull();
    });
    auto entry =
        std::ranges::find_if(this->consumers_, [consumer](const auto &p) {
            return p.widget == consumer;
        });
    if (entry == this->consumers_.end())
    {
        this->consumers_.push_back({consumer, QRegion(region)});
    }
    else
    {
        entry->region += region;
    }
    this->schedule();
}

void GlassBackdrop::schedule()
{
    if (this->dirty_ && !this->timer_.isActive() && captureDepth == 0 &&
        nativeglass::effectsAllowed() &&
        std::ranges::any_of(this->consumers_, [](const auto &p) {
            return p.widget && p.widget->isVisible() &&
                   !p.widget->window()->isMinimized();
        }))
    {
        this->timer_.start();
    }
}

bool GlassBackdrop::eventFilter(QObject *object, QEvent *event)
{
    if (object == this->source_ && captureDepth == 0)
    {
        if (event->type() == QEvent::FocusIn)
        {
            activeSource = this->source_;
        }
        if (event->type() == QEvent::Paint || event->type() == QEvent::Resize ||
            event->type() == QEvent::Show)
        {
            this->dirty_ = true;
            this->schedule();
        }
        if (event->type() == QEvent::Hide)
        {
            this->timer_.stop();
            this->top_ = {};
            this->bottom_ = {};
            ++this->generation_;
        }
    }
    return QObject::eventFilter(object, event);
}

QImage GlassBackdrop::diffuse(const QImage &source, int radius,
                              const QSize &maximum)
{
    if (source.isNull() || maximum.isEmpty())
    {
        return {};
    }
    const auto target =
        source.width() <= maximum.width() && source.height() <= maximum.height()
            ? source.size()
            : source.size().scaled(maximum, Qt::KeepAspectRatio);
    auto image =
        source.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_RGB32);
    radius = std::clamp(radius, 1, 16);
    // Three separable box passes approximate Gaussian diffusion. This operates
    // only on tiny chrome source strips, never on the scrolling reading layer.
    for (int pass = 0; pass < 3; ++pass)
    {
        QImage intermediate(image.size(), QImage::Format_RGB32);
        QImage result(image.size(), QImage::Format_RGB32);
        const int count = radius * 2 + 1;
        for (int y = 0; y < image.height(); ++y)
        {
            const auto *row =
                reinterpret_cast<const QRgb *>(image.constScanLine(y));
            auto *out = reinterpret_cast<QRgb *>(intermediate.scanLine(y));
            int red = 0, green = 0, blue = 0;
            for (int k = -radius; k <= radius; ++k)
            {
                const auto color = row[std::clamp(k, 0, image.width() - 1)];
                red += qRed(color);
                green += qGreen(color);
                blue += qBlue(color);
            }
            for (int x = 0; x < image.width(); ++x)
            {
                out[x] = qRgb(red / count, green / count, blue / count);
                const auto old =
                    row[std::clamp(x - radius, 0, image.width() - 1)];
                const auto next =
                    row[std::clamp(x + radius + 1, 0, image.width() - 1)];
                red += qRed(next) - qRed(old);
                green += qGreen(next) - qGreen(old);
                blue += qBlue(next) - qBlue(old);
            }
        }
        for (int x = 0; x < image.width(); ++x)
        {
            int red = 0, green = 0, blue = 0;
            const auto at = [&](int y) {
                return reinterpret_cast<const QRgb *>(
                    intermediate.constScanLine(
                        std::clamp(y, 0, image.height() - 1)))[x];
            };
            for (int k = -radius; k <= radius; ++k)
            {
                const auto color = at(k);
                red += qRed(color);
                green += qGreen(color);
                blue += qBlue(color);
            }
            for (int y = 0; y < image.height(); ++y)
            {
                reinterpret_cast<QRgb *>(result.scanLine(y))[x] =
                    qRgb(red / count, green / count, blue / count);
                const auto old = at(y - radius);
                const auto next = at(y + radius + 1);
                red += qRed(next) - qRed(old);
                green += qGreen(next) - qGreen(old);
                blue += qBlue(next) - qBlue(old);
            }
        }
        image = std::move(result);
    }
    return image;
}

void GlassBackdrop::refresh()
{
    if (!this->source_ || !this->source_->isVisible() ||
        this->source_->window()->isMinimized() ||
        !nativeglass::effectsAllowed())
    {
        return;
    }
    const int height = std::min(40, this->source_->height());
    if (height <= 0 || this->source_->width() <= 0)
    {
        return;
    }
    ++captureDepth;
    const auto top =
        diffuse(this->source_->grab(QRect(0, 0, this->source_->width(), height))
                    .toImage());
    const auto bottom =
        diffuse(this->source_
                    ->grab(QRect(0, this->source_->height() - height,
                                 this->source_->width(), height))
                    .toImage());
    --captureDepth;
    ++this->captures_;
    this->dirty_ = false;
    if (top != this->top_ || bottom != this->bottom_)
    {
        this->top_ = top;
        this->bottom_ = bottom;
        ++this->generation_;
        for (const auto &consumer : this->consumers_)
        {
            if (consumer.widget && consumer.widget->isVisible())
            {
                consumer.widget->update(consumer.region);
            }
        }
    }
}

const QImage &GlassBackdrop::image(GlassEdge edge) const
{
    return edge == GlassEdge::Top ? this->top_ : this->bottom_;
}
QWidget *GlassBackdrop::sourceWidget() const
{
    return this->source_;
}
quint64 GlassBackdrop::generation() const
{
    return this->generation_;
}
quint64 GlassBackdrop::captureCount() const
{
    return this->captures_;
}
quint64 GlassSurface::renderCount() const
{
    return this->renders_;
}

QImage GlassSurface::render(const QImage &backdrop, const QSize &size,
                            qreal ratio, const GlassOptions &options,
                            bool opticalEffects)
{
    if (size.isEmpty() || ratio <= 0)
    {
        return {};
    }
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    const auto &ui = uiPalette(options.light);
    const float width = size.width() / ratio;
    const float height = size.height() / ratio;
    const float radius =
        std::clamp<float>(options.radius, 0, std::min(width, height) * 0.5F);
    const float bevelWidth = options.role == GlassRole::Popover ? 7.0F : 4.5F;
    const float tint = options.light ? 0.82F : 0.76F;
    const float alpha =
        options.nativeBackdrop && opticalEffects ? 0.68F : 0.98F;
    const bool hasSource = !backdrop.isNull() && opticalEffects;
    for (int y = 0; y < size.height(); ++y)
    {
        auto *row = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < size.width(); ++x)
        {
            const float px = (x + 0.5F) / ratio;
            const float py = (y + 0.5F) / ratio;
            const float dx =
                std::abs(px - width * 0.5F) - width * 0.5F + radius;
            const float dy =
                std::abs(py - height * 0.5F) - height * 0.5F + radius;
            const float qx = std::max(dx, 0.0F);
            const float qy = std::max(dy, 0.0F);
            const float length = std::sqrt(qx * qx + qy * qy);
            const float distance =
                length + std::min(std::max(dx, dy), 0.0F) - radius;
            const float coverage = std::clamp(
                0.5F - distance * static_cast<float>(ratio), 0.0F, 1.0F);
            if (coverage == 0)
            {
                row[x] = 0;
                continue;
            }
            float nx = length > 0.001F ? qx / length : (dx > dy ? 1.0F : 0.0F);
            float ny = length > 0.001F ? qy / length : (dy >= dx ? 1.0F : 0.0F);
            nx *= px < width * 0.5F ? -1 : 1;
            ny *= py < height * 0.5F ? -1 : 1;
            const float bevel =
                opticalEffects
                    ? std::clamp(1.0F + distance / bevelWidth, 0.0F, 1.0F)
                    : 0;
            // A rounded slab normal displaces the actual diffuse source near its
            // rim. The 1.46 index of refraction and 2px limit keep lensing small.
            const float displacement =
                bevel * bevel * (2.0F / 1.46F) * options.refraction;
            QRgb under = ui.surface.rgb();
            if (hasSource)
            {
                under =
                    sample(backdrop,
                           (px + nx * displacement) * (backdrop.width() - 1) /
                               std::max(width, 1.0F),
                           (py + ny * displacement) * (backdrop.height() - 1) /
                               std::max(height, 1.0F));
            }
            const float facing =
                std::clamp(-nx * 0.45F - ny * 0.85F, -1.0F, 1.0F);
            const float fresnel = bevel * bevel * bevel;
            const float edge = opticalEffects
                                   ? fresnel * (std::max(facing, 0.0F) * 28.0F -
                                                std::max(-facing, 0.0F) * 15.0F)
                                   : 0;
            const float rim = opticalEffects && distance > -1.0F
                                  ? (options.light ? -9.0F : 10.0F) *
                                        (1 + std::max(facing, 0.0F))
                                  : 0;
            unsigned grainHash = static_cast<unsigned>(x) * 374761393U ^
                                 static_cast<unsigned>(y) * 668265263U;
            grainHash = (grainHash ^ (grainHash >> 13)) * 1274126177U;
            grainHash ^= grainHash >> 16;
            const float grain =
                opticalEffects ? ((grainHash & 255) / 255.0F - 0.5F) * 0.7F : 0;
            const auto channel = [&](int value, int base) {
                const float lit = mix(value, base, hasSource ? tint : 0.0F) +
                                  edge + rim + grain;
                const float active =
                    options.active ? lit : mix(lit, base, 0.36F);
                return qRound(std::clamp(active, 0.0F, 255.0F));
            };
            const int opacity =
                qRound(coverage * (opticalEffects ? alpha : 1.0F) * 255);
            row[x] = qPremultiply(
                qRgba(channel(qRed(under), ui.surface.red()),
                      channel(qGreen(under), ui.surface.green()),
                      channel(qBlue(under), ui.surface.blue()), opacity));
        }
    }
    return image;
}

namespace {
class PopupSnapshot final : public QObject
{
public:
    explicit PopupSnapshot(QWidget *owner)
        : QObject(owner)
        , owner_(owner)
    {
        owner->installEventFilter(this);
    }
    QImage image;
    quint64 generation = 1;

    void request()
    {
        const QRect bounds(this->owner_->mapToGlobal(QPoint()),
                           this->owner_->size());
        if (this->queued_ ||
            (bounds == this->bounds_ && !this->image.isNull()) ||
            !this->owner_->isVisible() || !nativeglass::effectsAllowed())
        {
            return;
        }
        this->queued_ = true;
        QTimer::singleShot(0, this, [this] {
            this->queued_ = false;
            if (!this->owner_->isVisible())
                return;
            auto *backdrop = GlassBackdrop::forWidget(this->owner_);
            auto *source = backdrop ? backdrop->sourceWidget() : nullptr;
            if (!source || !source->isVisible())
                return;
            this->bounds_ = QRect(this->owner_->mapToGlobal(QPoint()),
                                  this->owner_->size());
            const QRect sourceBounds(source->mapToGlobal(QPoint()),
                                     source->size());
            const QRect overlap = this->bounds_.intersected(sourceBounds);
            QImage sampleImage(this->owner_->size().scaled(QSize(384, 256),
                                                           Qt::KeepAspectRatio),
                               QImage::Format_RGB32);
            sampleImage.fill(this->owner_->palette()
                                         .color(QPalette::WindowText)
                                         .lightness() < 128
                                 ? QColor(244, 244, 246)
                                 : QColor(20, 20, 22));
            if (!overlap.isEmpty())
            {
                ++captureDepth;
                const auto content =
                    source->grab(overlap.translated(-sourceBounds.topLeft()));
                --captureDepth;
                QPainter painter(&sampleImage);
                painter.scale(
                    sampleImage.width() / qreal(this->owner_->width()),
                    sampleImage.height() / qreal(this->owner_->height()));
                painter.drawPixmap(
                    QRectF(overlap.translated(-this->bounds_.topLeft())),
                    content, QRectF(content.rect()));
            }
            this->image =
                GlassBackdrop::diffuse(sampleImage, 5, QSize(384, 256));
            ++this->generation;
            this->owner_->update();
        });
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == this->owner_)
        {
            if (event->type() == QEvent::Hide)
            {
                this->image = {};
                this->bounds_ = {};
                ++this->generation;
            }
            else if (event->type() == QEvent::Show ||
                     event->type() == QEvent::Move ||
                     event->type() == QEvent::Resize)
            {
                this->request();
            }
        }
        return QObject::eventFilter(object, event);
    }

private:
    QWidget *owner_;
    QRect bounds_;
    bool queued_ = false;
};

PopupSnapshot *popupSnapshot(QWidget *owner)
{
    static QHash<QWidget *, QPointer<PopupSnapshot>> snapshots;
    if (auto *existing = snapshots.value(owner).data())
    {
        existing->request();
        return existing;
    }
    auto *snapshot = new PopupSnapshot(owner);
    snapshots.insert(owner, snapshot);
    QObject::connect(owner, &QObject::destroyed, snapshot, [owner] {
        snapshots.remove(owner);
    });
    snapshot->request();
    return snapshot;
}
}  // namespace

void GlassSurface::paint(QPainter &painter, QWidget *owner, const QRectF &rect,
                         const GlassOptions &options, QWidget *source)
{
    if (rect.isEmpty() || GlassBackdrop::isCapturing())
    {
        return;
    }
    auto *backdrop =
        source ? GlassBackdrop::watch(source) : GlassBackdrop::forWidget(owner);
    auto *snapshot =
        options.role == GlassRole::Popover ? popupSnapshot(owner) : nullptr;
    if (backdrop && !snapshot)
    {
        backdrop->addConsumer(owner, rect.toAlignedRect());
    }
    const auto generation = snapshot   ? snapshot->generation
                            : backdrop ? backdrop->generation()
                                       : 0;
    const auto ratio = std::min<qreal>(owner->devicePixelRatioF(),
                                       2048.0 / std::max(1.0, rect.width()));
    const QSize size(qCeil(rect.width() * ratio), qCeil(rect.height() * ratio));
    const bool effects = nativeglass::effectsAllowed();
    if (this->cache_.isNull() || this->size_ != size || this->ratio_ != ratio ||
        this->backdrop_ != backdrop || this->generation_ != generation ||
        !sameOptions(this->options_, options) || this->effects_ != effects)
    {
        this->cache_ =
            QPixmap::fromImage(render(snapshot   ? snapshot->image
                                      : backdrop ? backdrop->image(options.edge)
                                                 : QImage{},
                                      size, ratio, options, effects));
        this->size_ = size;
        this->ratio_ = ratio;
        this->options_ = options;
        this->backdrop_ = backdrop;
        this->generation_ = generation;
        this->effects_ = effects;
        ++this->renders_;
    }
    painter.drawPixmap(rect, this->cache_,
                       QRectF(QPointF(), this->cache_.size()));
}

void GlassSurface::paintInteraction(QPainter &painter, const QRectF &rect,
                                    qreal radius, qreal amount, bool pressed,
                                    bool light, const QPointF &pointer)
{
    if (amount <= 0 || rect.isEmpty())
    {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    painter.fillPath(
        path,
        light ? QColor(0, 0, 0, qRound(amount * (pressed ? 22 : 10)))
              : QColor(255, 255, 255, qRound(amount * (pressed ? 21 : 12))));
    QLinearGradient reflection(rect.topLeft(), rect.bottomRight());
    reflection.setColorAt(0, QColor(255, 255, 255, qRound(amount * 55)));
    reflection.setColorAt(
        std::clamp(pointer.x() / std::max(1.0, rect.width()), 0.15, 0.85),
        QColor(255, 255, 255, qRound(amount * 22)));
    reflection.setColorAt(1, QColor(0, 0, 0, qRound(amount * 28)));
    painter.setPen(QPen(QBrush(reflection), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.restore();
}

void paintGlassSurface(QPainter &painter, QWidget *owner, const QRectF &rect,
                       const GlassOptions &options, QWidget *chatSource)
{
    struct Cache : QObject {
        explicit Cache(QWidget *parent)
            : QObject(parent)
        {
        }
        std::array<GlassSurface, 3> surfaces;
    };
    static QHash<QWidget *, QPointer<Cache>> caches;
    auto *cache = caches.value(owner).data();
    if (!cache)
    {
        cache = new Cache(owner);
        caches.insert(owner, cache);
        QObject::connect(owner, &QObject::destroyed, cache, [owner] {
            caches.remove(owner);
        });
    }
    cache->surfaces[static_cast<size_t>(options.role)].paint(
        painter, owner, rect, options, chatSource);
}

}  // namespace chatterino
