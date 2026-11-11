// SPDX-FileCopyrightText: 2019 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/buttons/Button.hpp"

#include "common/UiPalette.hpp"
#include "singletons/Theme.hpp"
#include "util/FunctionEventFilter.hpp"
#include "widgets/glass/GlassChrome.hpp"

#include <QApplication>
#include <QDebug>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QTimer>

namespace chatterino {

Button::Button(BaseWidget *parent)
    : BaseWidget(parent)
{
    this->hoverAnimation_.setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(&this->hoverAnimation_, &QVariantAnimation::valueChanged,
                     this, [this](const QVariant &value) {
                         this->hoverMultiplier_ = value.toReal();
                         this->update();
                     });
    this->setCursor(Qt::PointingHandCursor);
    this->setMouseTracking(true);
}

bool Button::enabled() const noexcept
{
    return this->enabled_;
}

void Button::setEnabled(bool enabled)
{
    this->enabled_ = enabled;
    this->setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    this->animateHover(this->mouseOver_);
    this->invalidateContent();
}

bool Button::mouseOver() const noexcept
{
    return this->mouseOver_;
}

bool Button::leftMouseButtonDown() const noexcept
{
    return this->leftMouseButtonDown_;
}

bool Button::menuVisible() const noexcept
{
    return this->menuVisible_;
}

QColor Button::borderColor() const noexcept
{
    return this->borderColor_;
}

void Button::setBorderColor(const QColor &color)
{
    this->borderColor_ = color;

    this->update();
}

std::optional<QColor> Button::mouseEffectColor() const
{
    return this->mouseEffectColor_;
}

void Button::setMouseEffectColor(std::optional<QColor> color)
{
    this->mouseEffectColor_ = color;
}

QMenu *Button::menu() const
{
    return this->menu_.get();
}

void Button::setMenu(std::unique_ptr<QMenu> menu)
{
    if (this->menu_)
    {
        this->menu_.release()->deleteLater();
    }

    this->menu_ = std::move(menu);
    this->menuVisible_ = this->menu_ && this->menu_->isVisible();
    if (!this->menu_)
    {
        return;
    }

    const QPointer<Button> self(this);
    this->menu_->installEventFilter(new FunctionEventFilter(
        this->menu_.get(), [self](QObject *watched, QEvent *event) {
            if (self && watched == self->menu_.get() &&
                event->type() == QEvent::Hide)
            {
                const QPointer<QMenu> menu(self->menu_.get());
                QTimer::singleShot(20, self.data(), [self, menu] {
                    if (menu && self->menu_.get() == menu && !menu->isVisible())
                    {
                        self->menuVisible_ = false;
                    }
                });
            }
            return false;
        }));
}

void Button::enableDrops(const std::vector<QString> &acceptedDropMimes_)
{
    this->setAcceptDrops(true);
    this->acceptedDropMimes = acceptedDropMimes_;
}

void Button::dragEnterEvent(QDragEnterEvent *event)
{
    auto anyMatches = std::ranges::any_of(
        this->acceptedDropMimes, [event](const QString &acceptedMime) {
            return event->mimeData()->hasFormat(acceptedMime);
        });

    if (!anyMatches)
    {
        return;
    }

    event->acceptProposedAction();

    this->addClickEffect(QPoint{
        this->width() / 2,
        this->height() / 2,
    });
}

void Button::paintEvent(QPaintEvent * )
{
    QPainter painter(this);
    this->paintButton(painter);
}

void Button::invalidateContent()
{
    this->pixmapValid_ = false;
    this->update();
}

bool Button::contentCacheEnabled() const noexcept
{
    return this->cachePixmap_;
}

void Button::setContentCacheEnabled(bool enabled)
{
    if (this->cachePixmap_ == enabled)
    {
        return;
    }

    if (!enabled)
    {
        this->cachedPixmap_ = {};
    }
    this->cachePixmap_ = enabled;
}

bool Button::opaqueContent() const noexcept
{
    return this->opaqueContent_;
}

void Button::setOpaqueContent(bool opaqueContent)
{
    this->opaqueContent_ = opaqueContent;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void Button::enterEvent(QEnterEvent * )
#else
void Button::enterEvent(QEvent * )
#endif
{
    if (!this->mouseOver_)
    {
        this->mouseOver_ = true;
        this->animateHover(this->mouseOver_);
        this->update();
        this->mouseOverUpdated();
    }
}

void Button::leaveEvent(QEvent * )
{
    if (this->mouseOver_)
    {
        this->mouseOver_ = false;
        this->animateHover(this->mouseOver_);
        this->update();
        this->mouseOverUpdated();
    }
}

void Button::mousePressEvent(QMouseEvent *event)
{
    if (!this->enabled_)
    {
        return;
    }

    switch (event->button())
    {
        case Qt::MouseButton::LeftButton: {
            const bool menuWasVisible = this->menuVisible_;
            this->leftMouseButtonDown_ = true;

            this->addClickEffect(event->pos());

            this->leftMousePress();

            if (this->menu_ && !menuWasVisible)
            {
                QTimer::singleShot(80, this, [this] {
                    this->showMenu();
                });
                this->leftMouseButtonDown_ = false;
                this->mouseOver_ = false;
            }
        }
        break;
        case Qt::MouseButton::RightButton: {
            this->rightMouseButtonDown_ = true;
        }
        break;
        case Qt::MouseButton::MiddleButton: {
            this->middleMouseButtonDown_ = true;
        }
        break;

        default:

            return;
    }
}

void Button::mouseReleaseEvent(QMouseEvent *event)
{

    bool hadCorrectButtonPressed = false;
    switch (event->button())
    {
        case Qt::MouseButton::LeftButton: {
            hadCorrectButtonPressed = this->leftMouseButtonDown_;
            this->leftMouseButtonDown_ = false;
        }
        break;
        case Qt::MouseButton::RightButton: {
            hadCorrectButtonPressed = this->rightMouseButtonDown_;
            this->rightMouseButtonDown_ = false;
        }
        break;
        case Qt::MouseButton::MiddleButton: {
            hadCorrectButtonPressed = this->middleMouseButtonDown_;
            this->middleMouseButtonDown_ = false;
        }
        break;

        default:

            return;
    }

    if (!this->enabled_)
    {
        return;
    }

    bool isInside = this->rect().contains(event->pos());

    if (isInside && hadCorrectButtonPressed)
    {
        if (event->button() == Qt::LeftButton)
        {
            this->leftClicked();
        }

        this->clicked(event->button());
    }
}

void Button::mouseMoveEvent(QMouseEvent *event)
{
    if (this->enabled_)
    {
        this->mousePos_ = event->pos();

        this->update();
    }
}

void Button::addClickEffect(QPoint)
{
    this->update();
}

void Button::animateHover(bool hovered)
{
    this->hoverAnimation_.stop();
    this->hoverAnimation_.setDuration(hovered ? 140 : 160);
    this->hoverAnimation_.setStartValue(this->hoverMultiplier_);
    this->hoverAnimation_.setEndValue(hovered && this->enabled_ ? 1.0 : 0.0);
    this->hoverAnimation_.start();
}

void Button::showMenu()
{
    if (!this->menu_)
    {
        return;
    }

    auto menuSizeHint = this->menu_->sizeHint();
    auto point = this->mapToGlobal(
        QPoint(this->width() - menuSizeHint.width(), this->height()));

    auto *screen = QApplication::screenAt(point);
    if (screen == nullptr)
    {
        screen = QApplication::primaryScreen();
    }
    auto bounds = screen->availableGeometry();

    if (point.y() + menuSizeHint.height() > bounds.bottom())
    {

        point.setY(point.y() - menuSizeHint.height() - this->height());
    }

    this->menu_->popup(point);
    this->menuVisible_ = true;
}

void Button::paintButton(QPainter &painter)
{
    painter.save();
    if (!this->enabled_ || !this->isEnabled())
    {
        painter.setOpacity(0.4);
    }
    this->fancyPaint(painter);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (this->cachePixmap_)
    {
        if (this->cachedPixmap_.size() / this->devicePixelRatio() !=
            this->size())
        {
            this->cachedPixmap_ =
                QPixmap(this->size() * this->devicePixelRatio());
            this->cachedPixmap_.setDevicePixelRatio(this->devicePixelRatio());
            this->pixmapValid_ = false;
        }

        if (!this->pixmapValid_)
        {
            if (!this->opaqueContent_)
            {
                this->cachedPixmap_.fill(Qt::transparent);
            }

            {
                QPainter pixmapPainter(&this->cachedPixmap_);
                this->paintContent(pixmapPainter);
            }

            this->pixmapValid_ = true;
        }
        // Disable smooth transformation, as we know the drawn pixmap has the
        // same size as the target area (in pixels).
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawPixmap(this->rect(), this->cachedPixmap_,
                           {{}, this->cachedPixmap_.size()});
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
    }
    else
    {
        this->paintContent(painter);
    }

    if (this->borderColor_.isValid())
    {
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(this->borderColor_);
        painter.drawRect(0, 0, this->width() - 1, this->height() - 1);
    }
    painter.restore();
}

void Button::fancyPaint(QPainter &painter)
{
    if (!this->enabled_ || !this->isEnabled())
    {
        return;
    }
    if (glass::surface(this, QRectF(this->rect()).adjusted(2, 2, -2, -2),
                       "button",
                       this->leftMouseButtonDown_ || this->menuVisible_
                           ? 1.0
                           : this->hoverMultiplier_))
        return;
    const auto pressed = this->leftMouseButtonDown_ || this->menuVisible_;
    const auto amount = pressed ? 1.0 : this->hoverMultiplier_;
    if (amount <= 0)
    {
        return;
    }
    auto color = this->mouseEffectColor_.value_or(
        this->theme->isLightTheme() ? QColor(0, 0, 0) : QColor(255, 255, 255));
    color.setAlphaF((pressed ? 0.12 : 0.065) * amount);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const qreal inset = std::max<qreal>(1, this->scale());
    const qreal radius = 5 * this->scale();
    painter.drawRoundedRect(
        QRectF(this->rect()).adjusted(inset, inset, -inset, -inset), radius,
        radius);
    painter.restore();
}

}
