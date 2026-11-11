// SPDX-License-Identifier: MIT
#pragma once
#include <QPainter>
#include <QRectF>
#include <QtPlugin>
namespace chatterino::glass {
enum class Surface : int { Composer, ChannelHeader, Tab };
class GlassSkin {
public:
    virtual ~GlassSkin() = default;
    virtual void paint(QPainter &painter, const QRectF &rect, Surface surface,
                       qreal emphasis, qreal scale) = 0;
};
}
#define MOLTORINO_GLASS_SKIN_IID "org.moltorino.GlassSkin/1.0"
Q_DECLARE_INTERFACE(chatterino::glass::GlassSkin, MOLTORINO_GLASS_SKIN_IID)
