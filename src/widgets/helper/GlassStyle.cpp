// SPDX-License-Identifier: MIT
#include "widgets/helper/GlassStyle.hpp"

#include "common/UiPalette.hpp"
#include "widgets/helper/GlassMaterial.hpp"

#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>

#include <algorithm>

namespace chatterino {
GlassStyle::GlassStyle()
    : QProxyStyle(QStyleFactory::create("Fusion"))
{
}

void GlassStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    if (qobject_cast<QMenu *>(widget) || widget->windowType() == Qt::ToolTip)
    {
        // A Qt popup remains a popup: keyboard navigation, grabs and actions
        // belong to QMenu. Only its backing surface becomes rounded glass.
        widget->setWindowFlag(Qt::FramelessWindowHint);
        widget->setAttribute(Qt::WA_TranslucentBackground);
        widget->setAutoFillBackground(false);
    }
}

int GlassStyle::pixelMetric(PixelMetric metric, const QStyleOption *option,
                            const QWidget *widget) const
{
    if (metric == PM_MenuPanelWidth || metric == PM_ToolTipLabelFrameWidth)
    {
        return 5;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

QSize GlassStyle::sizeFromContents(ContentsType type,
                                   const QStyleOption *option,
                                   const QSize &size,
                                   const QWidget *widget) const
{
    auto result = QProxyStyle::sizeFromContents(type, option, size, widget);
    if (type == CT_MenuItem)
    {
        const auto *item =
            qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (item && item->menuItemType != QStyleOptionMenuItem::Separator)
        {
            result.setHeight(
                std::max(result.height(), option->fontMetrics.height() + 10));
            result.rwidth() += 12;
        }
    }
    return result;
}

void GlassStyle::drawPrimitive(PrimitiveElement element,
                               const QStyleOption *option, QPainter *painter,
                               const QWidget *widget) const
{
    if (widget && (element == PE_PanelMenu || element == PE_PanelTipLabel))
    {
        auto *owner = const_cast<QWidget *>(widget);
        paintGlassSurface(
            *painter, owner, QRectF(option->rect),
            {.role = GlassRole::Popover,
             .radius = 8,
             .light = option->palette.color(QPalette::WindowText).lightness() <
                      128});
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void GlassStyle::drawControl(ControlElement element, const QStyleOption *option,
                             QPainter *painter, const QWidget *widget) const
{
    const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (element == CE_MenuItem && item)
    {
        const auto &ui = uiPalette(
            item->palette.color(QPalette::WindowText).lightness() < 128);
        if (item->menuItemType == QStyleOptionMenuItem::Separator &&
            item->text.isEmpty())
        {
            painter->fillRect(
                QRect(item->rect.left() + 10, item->rect.center().y(),
                      std::max(0, item->rect.width() - 20), 1),
                ui.border);
            return;
        }
        QStyleOptionMenuItem copy(*item);
        copy.palette.setColor(QPalette::Text, ui.text);
        copy.palette.setColor(QPalette::ButtonText, ui.text);
        copy.palette.setColor(QPalette::Disabled, QPalette::Text, ui.muted);
        copy.palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                              ui.muted);
        if (copy.state.testFlag(State_Selected) &&
            copy.state.testFlag(State_Enabled))
        {
            GlassSurface::paintInteraction(
                *painter, QRectF(copy.rect).adjusted(1, 0, -1, 0), 4, 1, false,
                ui.canvas.lightness() > 128, copy.rect.center());
            copy.state &= ~State_Selected;
        }
        QProxyStyle::drawControl(element, &copy, painter, widget);
        return;
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}
}  // namespace chatterino
