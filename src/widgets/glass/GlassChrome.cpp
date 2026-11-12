// SPDX-License-Identifier: MIT
#include "widgets/glass/GlassChrome.hpp"
#ifdef MOLTORINO_WITH_QTQUICK_GLASS_PREVIEW

#    include "Application.hpp"
#    include "singletons/Theme.hpp"
#    include "singletons/ThemeCustomization.hpp"
#    include "singletons/ThemeVideo.hpp"
#    include "singletons/ThemeVideoDecoder.hpp"
#    include "widgets/AccountSwitchPopup.hpp"
#    include "widgets/BasePopup.hpp"
#    include "widgets/buttons/Button.hpp"
#    include "widgets/dialogs/EmotePopup.hpp"
#    include "widgets/dialogs/SettingsDialog.hpp"
#    include "widgets/dialogs/UserInfoPopup.hpp"
#    include "widgets/helper/ChannelView.hpp"
#    include "widgets/helper/NotebookTab.hpp"
#    include "widgets/splits/Split.hpp"

#    include <QAbstractButton>
#    include <QAbstractItemModel>
#    include <QAbstractScrollArea>
#    include <QApplication>
#    include <QComboBox>
#    include <QCryptographicHash>
#    include <QDebug>
#    include <QElapsedTimer>
#    include <QEvent>
#    include <QFile>
#    include <QImage>
#    include <QHash>
#    include <QLabel>
#    include <QRegularExpression>
#    include <QPlainTextEdit>
#    include <QTextEdit>
#    include <QJsonArray>
#    include <QLineEdit>
#    include <QMenu>
#    include <QMouseEvent>
#    include <QPainter>
#    include <QPointer>
#    include <QQmlContext>
#    include <QQmlEngine>
#    include <QQuickImageProvider>
#    include <QQuickItem>
#    include <QQuickWindow>
#    include <QScrollArea>
#    include <QSGRendererInterface>
#    include <QStyle>
#    include <QTimer>
#    include <QtQuickWidgets/QQuickWidget>
#    include <QWindow>

#    include <algorithm>
#    include <memory>
#    include <optional>

namespace chatterino::glass {
namespace {
struct Panel {
    QPointer<QWidget> owner;
    QRectF local;
    QString kind;
    qreal emphasis = 0;
};
class Panels final : public QAbstractListModel
{
public:
    enum {
        Bounds = Qt::UserRole + 1,
        Kind,
        Emphasis,
        Hovered,
        Pressed,
        Disabled,
        Pointer
    };
    QList<Panel> entries;
    QPoint cursor;
    QWidget *window = nullptr;
    int rowCount(const QModelIndex & = {}) const override
    {
        return entries.size();
    }
    QHash<int, QByteArray> roleNames() const override
    {
        return {{Bounds, "bounds"},     {Kind, "kind"},
                {Emphasis, "emphasis"}, {Hovered, "hovered"},
                {Pressed, "pressed"},   {Disabled, "disabled"},
                {Pointer, "pointer"}};
    }
    QVariant data(const QModelIndex &i, int role) const override
    {
        if (!i.isValid() || i.row() >= entries.size())
            return {};
        const auto &p = entries[i.row()];
        if (!p.owner)
            return {};
        const auto rect =
            p.owner->window() == window
                ? QRectF(p.owner->mapTo(window, p.local.topLeft()),
                         p.local.size())
                : QRectF();
        const bool shown = p.owner->isVisibleTo(window) && !rect.isEmpty();
        const bool hover = shown && rect.contains(cursor);
        switch (role)
        {
            case Bounds:
                return shown ? rect : QRectF();
            case Kind:
                return p.kind;
            case Emphasis: {
                auto *focus = QApplication::focusWidget();
                if (p.kind == "composer")
                    return focus && (focus == p.owner ||
                                     p.owner->isAncestorOf(focus))
                               ? 1.
                               : 0.;
                return p.emphasis;
            }
            case Hovered:
                return hover;
            case Pressed: {
                if (auto *b = dynamic_cast<Button *>(p.owner.data()))
                    return b->leftMouseButtonDown() || b->menuVisible();
                if (auto *b = qobject_cast<QAbstractButton *>(p.owner.data()))
                    return b->isDown();
                return hover && (QApplication::mouseButtons() & Qt::LeftButton);
            }
            case Disabled: {
                auto *button = dynamic_cast<Button *>(p.owner.data());
                return !p.owner->isEnabled() || (button && !button->enabled());
            }
            case Pointer:
                return hover ? QPointF((cursor.x() - rect.x()) /
                                           std::max(1., rect.width()),
                                       (cursor.y() - rect.y()) /
                                           std::max(1., rect.height()))
                             : QPointF(-2, -2);
        }
        return {};
    }
    void put(QWidget *owner, QRectF rect, const QString &kind, qreal emphasis)
    {
        for (int i = 0; i < entries.size(); ++i)
        {
            auto &p = entries[i];
            if (p.owner == owner && p.kind == kind)
            {
                if (p.local != rect || p.emphasis != emphasis)
                {
                    p.local = rect;
                    p.emphasis = emphasis;
                    Q_EMIT dataChanged(index(i), index(i));
                }
                return;
            }
        }
        beginInsertRows({}, entries.size(), entries.size());
        entries.push_back({owner, rect, kind, emphasis});
        endInsertRows();
    }
    void refresh()
    {
        // Remove destroyed controls; animations and delegates are reused.
        for (int i = entries.size() - 1; i >= 0; --i)
            if (!entries[i].owner)
            {
                beginRemoveRows({}, i, i);
                entries.removeAt(i);
                endRemoveRows();
            }
        cursor = window->mapFromGlobal(QCursor::pos());
        if (!entries.isEmpty())
            Q_EMIT dataChanged(index(0), index(entries.size() - 1));
    }
};
class Backdrop final : public QQuickImageProvider
{
public:
    QImage image;
    Backdrop()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }
    QImage requestImage(const QString &, QSize *size, const QSize &) override
    {
        if (size)
            *size = image.size();
        return image;
    }
};
class Chrome;
QList<Chrome *> layers;
bool constructing = false;

bool preserveNativeSettings(const QWidget *window)
{
    return dynamic_cast<const SettingsDialog *>(window) != nullptr;
}

// Clear neutral dark panel fills over glass without recoloring semantic
// badges, warning states, progress indicators, or the Settings window.
QString clearChromeBackgrounds(QString sheet)
{
    static const QRegularExpression fill(
        QStringLiteral(R"((?<![-\w])background(?:-color)?\s*:\s*(#[0-9a-fA-F]{3,8})\s*;)"));
    static const QRegularExpression semantic(
        QStringLiteral("Swatch|ColorPreview|ColorPicker|Badge|Progress|Indicator"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression shell(
        QStringLiteral("#\\w*(?:Root|Content|Header|Footer|ScrollArea)(?:\\s|,|$)"));
    struct Replacement { qsizetype start; qsizetype length; QString value; };
    QList<Replacement> edits;
    auto hits = fill.globalMatch(sheet);
    while (hits.hasNext())
    {
        const auto hit = hits.next();
        const auto open = sheet.lastIndexOf('{', hit.capturedStart());
        const auto previous = sheet.lastIndexOf('}', open);
        const auto selector = sheet.mid(previous + 1, open - previous - 1);
        if (semantic.match(selector).hasMatch())
            continue;
        QColor color(hit.captured(1));
        if (!color.isValid() || color.alpha() < 240 ||
            color.lightnessF() > 0.32 || color.hsvSaturationF() > 0.25)
            continue;
        color.setAlpha(shell.match(selector).hasMatch() ? 0 : 55);
        edits.append({hit.capturedStart(1), hit.capturedLength(1),
                      QStringLiteral("rgba(%1,%2,%3,%4)")
                          .arg(color.red()).arg(color.green())
                          .arg(color.blue()).arg(color.alpha())});
    }
    for (auto it = edits.crbegin(); it != edits.crend(); ++it)
        sheet.replace(it->start, it->length, it->value);
    return sheet;
}

// QMenu and Qt's tooltip label paint foreground directly on their window.
// Replay that native paint ABOVE the GPU child without rendering children or
// replacing actions, shortcuts, accessibility, submenu logic or hit testing.
class NativeForeground final : public QWidget
{
    bool rendering_ = false;

public:
    explicit NativeForeground(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
    }
    bool rendering() const
    {
        return rendering_;
    }
    void paintEvent(QPaintEvent *) override
    {
        if (rendering_)
            return;
        rendering_ = true;
        QImage text(size() * devicePixelRatioF(),
                    QImage::Format_ARGB32_Premultiplied);
        text.setDevicePixelRatio(devicePixelRatioF());
        text.fill(Qt::transparent);
        parentWidget()->render(&text, {}, QRegion(),
                               QWidget::DrawWindowBackground);
        QPainter painter(this);
        painter.drawImage(QPoint(), text);
        rendering_ = false;
    }
};
class Chrome final : public QQuickWidget
{
public:
    Panels panels;
    Backdrop *backdrop = nullptr;
    QPointer<NativeForeground> foreground;
    QTimer synchronize;
    int revision = 0;
    bool failed = false;
    QSize backdropSize;
    QImage popupBackdrop;
    QPointer<QWidget> popupOwner;
    QTimer popupRefresh;
    QPoint popupBackdropOrigin;
    QString backdropKey;
    std::optional<ThemeCustomizationProfile> backdropProfile;
    QString videoSource;
    std::shared_ptr<ThemeVideo> video;
    QElapsedTimer videoThrottle;
    int frames = 0;
    QList<QRectF> wallpaperGeometry;
    QHash<QWidget *, QPair<QString, QString>> styled;
    QList<QPointer<QWidget>> clearedBackgrounds;
    QList<QPointer<ChannelView>> transparentViews;
    explicit Chrome(QWidget *parent)
        : QQuickWidget(parent)
    {
        // Popovers refract only pixels of their owning Moltorino window.
        // This capture stays in memory, is never saved, and never reads the desktop.
        if (!dynamic_cast<UserInfoPopup *>(parent) &&
            (parent->windowType() == Qt::Popup ||
            parent->windowType() == Qt::ToolTip ||
            qobject_cast<QMenu *>(parent) ||
            dynamic_cast<BasePopup *>(parent) ||
            dynamic_cast<AccountSwitchPopup *>(parent)))
        {
            auto *owner = parent->parentWidget()
                              ? parent->parentWidget()->window()
                              : QApplication::activeWindow();
            if (owner && owner != parent && owner->isVisible())
            {
                popupOwner = owner;
                capturePopupBackdrop();
            }
        }
        popupRefresh.setInterval(125);
        connect(&popupRefresh, &QTimer::timeout, this, [this] {
            if (isVisible() && !failed)
                capturePopupBackdrop();
        });
        if (popupOwner)
            popupRefresh.start();
        layers.push_back(this);
        setObjectName("glassorinoGpuChrome");
        if (auto *menu = qobject_cast<QMenu *>(parent))
        {
            menu->setAttribute(Qt::WA_TranslucentBackground);
            menu->setStyleSheet(menu->styleSheet() + QStringLiteral(
                "QMenu { background: rgba(22,24,29,45); "
                "border: 1px solid rgba(245,248,255,65); "
                "border-radius: 9px; padding: 4px; } "
                "QMenu::item { border-radius: 6px; padding: 5px 9px; } "
                "QMenu::item:selected { background: rgba(178,198,219,40); }"));
        }
        videoThrottle.start();
        connect(
            quickWindow(), &QQuickWindow::afterRendering, this,
            [this] {
                ++frames;
            },
            Qt::DirectConnection);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setClearColor(Qt::transparent);
        setResizeMode(QQuickWidget::SizeRootObjectToView);
        quickWindow()->setPersistentSceneGraph(false);
        connect(
            quickWindow(), &QQuickWindow::sceneGraphInitialized, this, [this] {
                if (quickWindow()->rendererInterface()->graphicsApi() ==
                    QSGRendererInterface::Software)
                    failRenderer(QStringLiteral("Software Qt Quick cannot "
                                                "execute the optical shaders"));
                else
                    parentWidget()->update();
            });
        panels.window = parent;
        backdrop = new Backdrop;
        engine()->addImageProvider("ownedBackdrop", backdrop);
        rootContext()->setContextProperty("chromePanels", &panels);
        connect(this, &QQuickWidget::sceneGraphError, this,
                [this](auto, const QString &message) {
                    failRenderer(message);
                });
        setSource(QUrl("qrc:/moltorino-glass/GlassChrome.qml"));
        if (status() == QQuickWidget::Error)
        {
            failRenderer(
                QStringLiteral("Production material QML failed to load"));
            qWarning() << "GLASS_QML_ERROR" << errors();
        }
        synchronize.setSingleShot(true);
        synchronize.setInterval(0);
        connect(&synchronize, &QTimer::timeout, this, [this] {
            sync();
        });
        setGeometry(parent->rect());
        lower();
        show();
        if (qobject_cast<QMenu *>(parent) || parent->inherits("QTipLabel"))
        {
            foreground = new NativeForeground(parent);
            foreground->setGeometry(parent->rect());
            foreground->raise();
            foreground->show();
        }
        request();
    }
    void capturePopupBackdrop()
    {
        if (!popupOwner || !popupOwner->isVisible() ||
            popupOwner->isMinimized())
            return;
        const auto origin =
            popupOwner->mapFromGlobal(parentWidget()->mapToGlobal(QPoint()));
        const auto region = QRect(origin, parentWidget()->size())
                                .intersected(popupOwner->rect());
        if (region.isEmpty())
            return;
        const auto offset = parentWidget()->mapFromGlobal(
            popupOwner->mapToGlobal(region.topLeft()));
        auto pixels = popupOwner->grab(region).toImage();
        if (pixels == popupBackdrop && offset == popupBackdropOrigin)
            return;
        popupBackdrop = std::move(pixels);
        popupBackdropOrigin = offset;
        backdropKey.clear();
        if (rootObject())
            request();
    }
    void failRenderer(const QString &message)
    {
        failed = true;
        popupRefresh.stop();
        qWarning() << "GLASS_GPU_ERROR" << message;
        QTimer::singleShot(0, this, [this] {
            hide();
            for (auto it = styled.cbegin(); it != styled.cend(); ++it)
                if (it.key() && it.key()->styleSheet() == it.value().second)
                    it.key()->setStyleSheet(it.value().first);
            for (const auto &w : clearedBackgrounds)
                if (w)
                    w->setAutoFillBackground(true);
            for (const auto &view : transparentViews)
                if (view)
                    view->setTransparentBackground(false);
            for (auto *w : parentWidget()->findChildren<QWidget *>())
            {
                w->setProperty("liquidGlassSurface", false);
                w->style()->unpolish(w);
                w->style()->polish(w);
            }
            if (foreground)
                foreground->hide();
            parentWidget()->setAutoFillBackground(true);
            const auto bg = getTheme()->window.background.name();
            parentWidget()->setStyleSheet(
                parentWidget()->styleSheet() +
                QString("QMenu,QToolTip,QPushButton,QToolButton,QLineEdit,"
                        "QComboBox {background:%1;}")
                    .arg(bg));
            parentWidget()->update();
        });
    }
    ~Chrome() override
    {
        disconnect(quickWindow(), nullptr, this, nullptr);
        if (video)
            video->detach(this);
        layers.removeAll(this);
    }
    void request()
    {
        if (!synchronize.isActive())
            synchronize.start();
    }
    void updateBackdrop()
    {
        const auto &profile = getTheme()->customization;
        if (videoSource != profile.wallpaperSource)
        {
            if (video)
                video->detach(this);
            video.reset();
            videoSource = profile.wallpaperSource;
            if (isThemeVideo(videoSource))
            {
                video = ThemeVideo::acquire(videoSource, this);
                connect(video.get(), &ThemeVideo::frameChanged, this, [this] {
                    if (videoThrottle.elapsed() >= 33)
                    {
                        videoThrottle.restart();
                        backdropKey.clear();
                        request();
                    }
                });
            }
        }
        const auto key = profile.wallpaperSource +
                         QString::number(profile.wallpaperOpacity) +
                         QString::number(profile.wallpaperOverlayOpacity) +
                         QString::number(profile.wallpaperBlur) +
                         getTheme()->window.background.name() +
                         QString::number(devicePixelRatioF());
        QList<QRectF> mapping;
        for (auto *view : parentWidget()->findChildren<ChannelView *>())
        {
            if (auto *split = dynamic_cast<Split *>(view->parentWidget());
                split && view->isVisible() && view->window() == parentWidget())
            {
                mapping << view->glassWallpaperTarget();
                mapping << QRectF(split->mapTo(parentWidget(), QPointF()),
                                  QSizeF(split->size()));
            }
        }
        if (backdropSize == size() && wallpaperGeometry == mapping &&
            backdropKey == key && backdropProfile &&
            *backdropProfile == profile)
            return;
        backdropSize = size();
        backdropKey = key;
        backdropProfile = profile;
        wallpaperGeometry = mapping;
        QImage image(size() * devicePixelRatioF(),
                     QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(devicePixelRatioF());
        const auto canvas = profile.hasWallpaper()
                                ? profile.chatBackground
                                : getTheme()->window.background;
        image.fill(canvas);
        QPainter p(&image);
        auto wallpaper = profile.wallpaperSource.isEmpty() || video
                             ? QImage()
                             : loadThemeWallpaper(profile.wallpaperSource);
        if (!wallpaper.isNull())
            wallpaper =
                blurThemeWallpaper(std::move(wallpaper), profile.wallpaperBlur);
        if (video || !wallpaper.isNull())
        {
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.setOpacity(profile.wallpaperOpacity / 100.);
            auto drawWallpaper = [&](const QRectF &target) {
                if (video)
                {
                    video->paint(p, target, profile);
                    return;
                }
                if (profile.wallpaperMode == ThemeWallpaperMode::Tile)
                {
                    const auto zoom =
                        std::clamp(profile.wallpaperZoom, 100, 300) / 100.;
                    p.save();
                    p.translate(target.topLeft());
                    p.scale(zoom, zoom);
                    const auto clip = p.clipBoundingRect();
                    // The tile origin stays identical to ChannelView's mapping.
                    const auto area = clip.isEmpty()
                                          ? QRectF(-target.topLeft() / zoom,
                                                   QSizeF(size()) / zoom)
                                          : clip;
                    p.drawTiledPixmap(area, QPixmap::fromImage(wallpaper),
                                      area.topLeft());
                    p.restore();
                    return;
                }
                const auto layout = layoutThemeWallpaper(
                    wallpaper.size(), target, profile.wallpaperMode,
                    profile.wallpaperFocalX, profile.wallpaperFocalY,
                    profile.wallpaperZoom);
                if (layout.source.isEmpty())
                    return;
                const qreal sx =
                    layout.destination.width() / layout.source.width();
                const qreal sy =
                    layout.destination.height() / layout.source.height();
                // Extend the same photo transform under the chrome, rather than
                // stretching a screenshot or re-cropping it on every surface.
                const QRectF extended(
                    layout.destination.topLeft() -
                        QPointF(layout.source.x() * sx, layout.source.y() * sy),
                    QSizeF(wallpaper.width() * sx, wallpaper.height() * sy));
                p.drawImage(extended, wallpaper);
            };
            drawWallpaper(mapping.isEmpty() ? QRectF(rect()) : mapping.front());
            for (int i = 0; i + 1 < mapping.size(); i += 2)
            {
                p.save();
                p.setClipRect(mapping[i + 1]);
                p.setOpacity(1.);
                p.fillRect(mapping[i + 1], canvas);
                p.setOpacity(profile.wallpaperOpacity / 100.);
                drawWallpaper(mapping[i]);
                p.restore();
            }
            p.setOpacity(1.);
            auto overlay = profile.wallpaperOverlayColor;
            overlay.setAlphaF(overlay.alphaF() *
                              profile.wallpaperOverlayOpacity / 100.);
            p.fillRect(rect(), overlay);
        }
        if (!popupBackdrop.isNull())
        {
            p.drawImage(popupBackdropOrigin, popupBackdrop);
            p.fillRect(rect(), QColor(0, 0, 0, 65));
        }
        p.end();
        backdrop->image = std::move(image);
        if (rootObject())
            rootObject()->setProperty(
                "backdropUrl",
                QString("image://ownedBackdrop/%1").arg(++revision));
    }
    void sync()
    {
        // Timers queued before shutdown must not fetch Themes/Windows after
        // Application::stop() has released their owning singletons.
        if (isAppAboutToQuit() || !tryGetApp())
            return;
        if (failed || !parentWidget()->isVisible())
            return;
        if (geometry() != parentWidget()->rect())
            setGeometry(parentWidget()->rect());
        updateBackdrop();
        const bool settings = preserveNativeSettings(parentWidget());
        if (!settings && parentWidget()->autoFillBackground())
            parentWidget()->setAutoFillBackground(false);
        if (!parentWidget()->inherits("chatterino::Window"))
            panels.put(parentWidget(),
                       parentWidget()->rect().adjusted(1, 1, -1, -1), "window",
                       0.);
        const auto controls = parentWidget()->findChildren<QWidget *>();
        for (auto *w : controls)
        {
            if (w->window() != parentWidget() || !w->isVisible() || w == this ||
                w == foreground || isAncestorOf(w))
                continue;
            if (auto *view = dynamic_cast<ChannelView *>(w))
            {
                // Preserve native chat text. A usercard history must reveal the
                // same wallpaper, never a second captured copy of the chat.
                if ((dynamic_cast<EmotePopup *>(parentWidget()) ||
                     dynamic_cast<UserInfoPopup *>(parentWidget())) &&
                    !view->getTransparentBackground())
                {
                    transparentViews.append(view);
                    view->setTransparentBackground(true);
                }
                continue;
            }
            bool inChat = false;
            for (auto *p = w->parentWidget(); p && p != parentWidget();
                 p = p->parentWidget())
                if (dynamic_cast<ChannelView *>(p))
                    inChat = true;
            if (inChat || settings)
                continue;
            if (w->autoFillBackground())
            {
                clearedBackgrounds.append(w);
                w->setAutoFillBackground(false);
            }
            if (auto *scroll = qobject_cast<QScrollArea *>(w))
            {
                scroll->viewport()->setAutoFillBackground(false);
                if (scroll->widget())
                    scroll->widget()->setAutoFillBackground(false);
            }
            const auto current = w->styleSheet();
            if (!current.isEmpty() &&
                (!styled.contains(w) || styled.value(w).second != current))
            {
                const auto material = clearChromeBackgrounds(current);
                if (!styled.contains(w))
                    connect(w, &QObject::destroyed, this,
                            [this, w] { styled.remove(w); });
                styled[w] = {current, material};
                if (material != current)
                    w->setStyleSheet(material);
            }
            if (dynamic_cast<Button *>(w) && !dynamic_cast<NotebookTab *>(w))
            {
                panels.put(w, QRectF(w->rect()).adjusted(2, 2, -2, -2),
                           "button", 0.);
            }
            else if (qobject_cast<QAbstractButton *>(w) &&
                     !w->inherits("QCheckBox") && !w->inherits("QRadioButton"))
            {
                panels.put(w, QRectF(w->rect()).adjusted(1, 1, -1, -1),
                           "button", 0.);
                if (!w->property("liquidGlassSurface").toBool())
                {
                    w->setProperty("liquidGlassSurface", true);
                    w->style()->unpolish(w);
                    w->style()->polish(w);
                }
            }
            else if (qobject_cast<QLineEdit *>(w) ||
                     qobject_cast<QComboBox *>(w))
            {
                panels.put(w, QRectF(w->rect()).adjusted(1, 1, -1, -1), "field",
                           w->hasFocus() ? 1. : 0.);
                if (!w->property("liquidGlassSurface").toBool())
                {
                    w->setProperty("liquidGlassSurface", true);
                    w->style()->unpolish(w);
                    w->style()->polish(w);
                }
            }
        }
        if (auto *menu = qobject_cast<QMenu *>(parentWidget()))
        {
            auto *active = menu->activeAction();
            panels.put(menu, active ? menu->actionGeometry(active) : QRectF(),
                       "selection", 1.);
        }
        panels.refresh();
        lower();
        if (foreground)
        {
            foreground->setGeometry(parentWidget()->rect());
            foreground->raise();
            foreground->update();
        }
    }
};
Chrome *layer(const QWidget *widget)
{
    if (!widget)
        return nullptr;
    for (auto *c : layers)
        if (c->parentWidget() == widget->window())
            return c;
    return nullptr;
}
class ChromeEvents final : public QObject
{
    bool eventFilter(QObject *object, QEvent *event) override
    {
        // Do not install additional GPU windows or schedule material refresh
        // while the underlying Qt application is already closing.
        if (isAppAboutToQuit() || !tryGetApp())
            return false;
        auto *w = qobject_cast<QWidget *>(object);
        if (!w || constructing || w->inherits("QQuickWidget") ||
            w->objectName() == "glassorinoGpuChrome")
            return false;
        if (event->type() == QEvent::Show && w->isWindow() && !layer(w) &&
            !w->inherits("QDesktopWidget") &&
            (w->windowFlags() & Qt::WindowType_Mask) != Qt::ForeignWindow)
        {
            // Never change composition of embedded browser/OS-owned windows.
            constructing = true;
            new Chrome(w);
            constructing = false;
        }
        auto *c = layer(w);
        if (!c || c->isAncestorOf(w) || w == c->foreground)
            return false;
        if (w == c->parentWidget())
        {
            if (event->type() == QEvent::Hide)
                c->popupRefresh.stop();
            if (event->type() == QEvent::Show && c->popupOwner)
                c->popupRefresh.start();
        }
        switch (event->type())
        {
            case QEvent::Resize:
            case QEvent::Move:
            case QEvent::Show:
            case QEvent::Hide:
            case QEvent::EnabledChange:
            case QEvent::FocusIn:
            case QEvent::FocusOut:
            case QEvent::Enter:
            case QEvent::Leave:
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::ActionChanged:
            case QEvent::Wheel:
            case QEvent::LayoutRequest:
            case QEvent::PaletteChange:
                c->request();
                break;
            default:
                break;
        }
        return false;
    }
};
}  // namespace
bool enabled()
{
#    ifdef MOLTORINO_WITH_QTQUICK_GLASS_PREVIEW
    return !qEnvironmentVariableIsSet("MOLTORINO_GLASS_DISABLE") &&
           qEnvironmentVariable("QT_QUICK_BACKEND").trimmed().compare(
               QStringLiteral("software"), Qt::CaseInsensitive) != 0;
#    else
    return false;
#    endif
}
void shutdownChrome()
{
    // Main Moltorino windows aren't WA_DeleteOnClose. Release GPU resources
    // deterministically while Qt Quick/Gui are still initialized, instead
    // of leaving QQuickWidgets alive until QApplication destruction.
    const auto active = layers;
    for (auto *chrome : active)
        delete chrome;  // ~Chrome() removes itself from layers.
}
void installChrome()
{
    if (!enabled())
        return;
    auto *events = new ChromeEvents;
    events->setParent(qApp);
    qApp->installEventFilter(events);
}
bool available(const QWidget *widget)
{
    auto *c = layer(widget);
    return enabled() && c && !c->failed;
}
bool surface(QWidget *widget, const QRectF &rect, const char *kind,
             qreal emphasis)
{
    auto *c = layer(widget);
    if (!available(widget))
        return false;
    c->panels.put(widget, rect, QString::fromLatin1(kind), emphasis);
    return true;
}
void reviewOptics(QWidget *window, bool active)
{
    if (qEnvironmentVariableIsEmpty("MOLTORINO_UI_REVIEW_DIR"))
        return;
    if (auto *c = layer(window); c && c->rootObject())
        c->rootObject()->setProperty("opticsEnabled", active);
}
QJsonObject diagnostics(QWidget *window)
{
    auto *c = layer(window);
    QJsonObject result{{"enabled", enabled()},
                       {"available", available(window)}};
    if (!c)
        return result;
    QJsonArray entries;
    for (int i = 0; i < c->panels.entries.size(); ++i)
    {
        const auto index = c->panels.index(i);
        const auto rect = c->panels.data(index, Panels::Bounds).toRectF();
        if (rect.isEmpty())
            continue;
        entries.append(QJsonObject{{"kind", c->panels.entries[i].kind},
                                   {"x", rect.x()},
                                   {"y", rect.y()},
                                   {"width", rect.width()},
                                   {"height", rect.height()}});
    }
    result["surfaces"] = entries;
    result["graphicsApi"] =
        static_cast<int>(c->quickWindow()->rendererInterface()->graphicsApi());
    result["qmlReady"] = c->status() == QQuickWidget::Ready;
    result["backdropRevision"] = c->revision;
    result["renderedFrames"] = c->frames;
    QJsonObject baked;
    for (const auto &name : {"glass.vert.qsb", "glass.frag.qsb"})
    {
        QFile file(QString(":/shaders/") + name);
        if (file.open(QIODevice::ReadOnly))
            baked[name] = QString::fromLatin1(
                QCryptographicHash::hash(file.readAll(),
                                         QCryptographicHash::Sha256)
                    .toHex());
    }
    result["bakedShaderSha256"] = baked;
    return result;
}
}  // namespace chatterino::glass

#else
namespace chatterino::glass {
void shutdownChrome()
{
}
void installChrome()
{
}
bool enabled()
{
    return false;
}
bool available(const QWidget *)
{
    return false;
}
bool surface(QWidget *, const QRectF &, const char *, qreal)
{
    return false;
}
void reviewOptics(QWidget *, bool)
{
}
QJsonObject diagnostics(QWidget *)
{
    return {{"enabled", false}};
}
}  // namespace chatterino::glass
#endif
