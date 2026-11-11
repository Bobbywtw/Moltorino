// SPDX-License-Identifier: MIT
#include "widgets/glass/GlassSkinInterface.hpp"
#include <QColor>
#include <QLinearGradient>
#include <QObject>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
namespace chatterino::glass {
class LiquidGlassSkin final : public QObject, public GlassSkin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID MOLTORINO_GLASS_SKIN_IID)
    Q_INTERFACES(chatterino::glass::GlassSkin)
public:
    void paint(QPainter &p, const QRectF &bounds, Surface s,
               qreal emphasis, qreal scale) override {
        if (bounds.width() < 2 || bounds.height() < 2) return;
        const qreal z = std::max<qreal>(0.5, scale);
        const qreal active = std::clamp<qreal>(emphasis, 0.0, 1.0);
        const QRectF r = bounds.adjusted(z * 0.7, z * 0.7, -z * 0.7, -z * 0.7);
        const qreal corner = std::min(r.height() / 2,
            (s == Surface::Composer ? 13.0 : s == Surface::Tab ? 8.0 : 9.0) * z);
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath shape;
        shape.addRoundedRect(r, corner, corner);
        // Optical-material first milestone. Real pixel refraction and
        // backdrop diffusion are explicitly NOT implemented in this pass.
        QLinearGradient fill(r.topLeft(), r.bottomLeft());
        const int alpha = s == Surface::Tab ? int(14 + 37 * active) : 35;
        fill.setColorAt(0.0, QColor(241, 246, 255, alpha + int(8 * active)));
        fill.setColorAt(0.46, QColor(170, 186, 209, alpha / 2));
        fill.setColorAt(1.0, QColor(31, 38, 51, alpha / 2));
        p.fillPath(shape, fill);
        QLinearGradient rim(r.topLeft(), r.bottomLeft());
        rim.setColorAt(0.0, QColor(255, 255, 255, 130 + int(35 * active)));
        rim.setColorAt(0.30, QColor(243, 248, 255, 46));
        rim.setColorAt(0.70, QColor(190, 204, 221, 18));
        rim.setColorAt(1.0, QColor(243, 248, 255, 75));
        p.setPen(QPen(QBrush(rim), std::max<qreal>(0.75, z * 0.8)));
        p.setBrush(Qt::NoBrush);
        p.drawPath(shape);
        const qreal inset = z * 1.5;
        QRectF inner = r.adjusted(inset, inset, -inset, -inset);
        if (inner.width() > 3 && inner.height() > 3) {
            QPainterPath inside;
            inside.addRoundedRect(inner, std::max(1.0, corner - inset),
                                  std::max(1.0, corner - inset));
            QLinearGradient highlight(inner.topLeft(), inner.bottomLeft());
            highlight.setColorAt(0.0, QColor(255, 255, 255, 39 + int(15 * active)));
            highlight.setColorAt(0.5, QColor(255, 255, 255, 5));
            highlight.setColorAt(1.0, QColor(255, 255, 255, 0));
            p.setPen(QPen(QBrush(highlight), std::max<qreal>(0.6, z * 0.55)));
            p.drawPath(inside);
        }
        p.restore();
    }
};
}
#include "LiquidGlassSkin.moc"
