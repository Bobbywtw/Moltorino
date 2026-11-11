// SPDX-License-Identifier: MIT
#include "widgets/glass/GlassSkinHost.hpp"

#include "widgets/glass/GlassChrome.hpp"

#include <QWidget>
namespace chatterino::glass {
bool paint(QPainter &painter, const QRectF &rect, Surface kind, qreal emphasis,
           qreal)
{
    auto *widget = dynamic_cast<QWidget *>(painter.device());
    const char *name = kind == Surface::Composer        ? "composer"
                       : kind == Surface::ChannelHeader ? "header"
                                                        : "tab";
    return surface(widget, rect, name, emphasis);
}
}
