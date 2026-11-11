// SPDX-License-Identifier: MIT
#pragma once

#include <QHash>
#include <QProxyStyle>

#include <memory>

namespace chatterino {
class GlassSurface;

class GlassStyle final : public QProxyStyle
{
public:
    GlassStyle();
    void polish(QWidget *widget) override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option,
                           const QSize &size,
                           const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget) const override;
    void drawControl(ControlElement element, const QStyleOption *option,
                     QPainter *painter, const QWidget *widget) const override;
};
}  // namespace chatterino
