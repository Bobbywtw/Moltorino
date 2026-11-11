// SPDX-License-Identifier: MIT
#pragma once

#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QRectF>
#include <QRegion>
#include <QSize>
#include <QTimer>

#include <vector>

class QPainter;
class QWidget;

namespace chatterino {

enum class GlassEdge { Top, Bottom };
enum class GlassRole { Chrome, Composer, Popover };

// One sampler per chat view, shared by its chrome. Only app-owned, clipped
// strips are sampled; never the desktop, other applications or account fields.
class GlassBackdrop final : public QObject
{
public:
    static GlassBackdrop *watch(QWidget *chatView);
    static GlassBackdrop *forWidget(QWidget *consumer);
    static bool isCapturing();

    void addConsumer(QWidget *consumer, const QRect &region);
    const QImage &image(GlassEdge edge) const;
    quint64 generation() const;
    quint64 captureCount() const;
    static QImage diffuse(const QImage &source, int radius = 5,
                          const QSize &maximum = QSize(384, 32));
    QWidget *sourceWidget() const;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    explicit GlassBackdrop(QWidget *source);
    void refresh();
    void schedule();

    QPointer<QWidget> source_;
    struct Consumer {
        QPointer<QWidget> widget;
        QRegion region;
    };
    std::vector<Consumer> consumers_;
    QTimer timer_;
    QImage top_;
    QImage bottom_;
    quint64 generation_ = 1;
    quint64 captures_ = 0;
    bool dirty_ = true;
};

struct GlassOptions {
    GlassRole role = GlassRole::Chrome;
    GlassEdge edge = GlassEdge::Top;
    qreal radius = 0;
    bool light = false;
    bool active = true;
    bool nativeBackdrop = false;
    float refraction = 1.0F;
};

// Cached optical layer: diffuse transmission, rounded edge normals, bounded
// refraction, Fresnel reflection and fine grain. Foreground text/icons remain
// ordinary Qt painting and are never blurred or displaced.
class GlassSurface
{
public:
    void paint(QPainter &painter, QWidget *owner, const QRectF &rect,
               const GlassOptions &options, QWidget *chatSource = nullptr);
    static QImage render(const QImage &diffuseBackdrop, const QSize &pixelSize,
                         qreal pixelRatio, const GlassOptions &options,
                         bool opticalEffects = true);
    static void paintInteraction(QPainter &painter, const QRectF &rect,
                                 qreal radius, qreal amount, bool pressed,
                                 bool light, const QPointF &pointer);
    quint64 renderCount() const;

private:
    QPixmap cache_;
    QSize size_;
    qreal ratio_ = 0;
    GlassOptions options_;
    QPointer<GlassBackdrop> backdrop_;
    quint64 generation_ = 0;
    quint64 renders_ = 0;
    bool effects_ = true;
};

// Widget-owned caches keep existing widget headers/layouts unchanged.
void paintGlassSurface(QPainter &painter, QWidget *owner, const QRectF &rect,
                       const GlassOptions &options,
                       QWidget *chatSource = nullptr);

}  // namespace chatterino
