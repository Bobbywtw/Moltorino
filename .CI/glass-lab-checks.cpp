// SPDX-License-Identifier: MIT
// Exercise the real QML components and baked GPU shaders, without app state.
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSGRendererInterface>
#include <QTimer>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {
void settle(int milliseconds = 350)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

bool save(const QImage &image, const QString &filename)
{
    return !image.isNull() && image.save(filename);
}
}  // namespace

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    if (argc != 2)
    {
        return 2;
    }
    const QString output = QString::fromLocal8Bit(argv[1]);
    QDir().mkpath(output);
    for (const auto *shader :
         {":/shaders/glass.vert.qsb", ":/shaders/glass.frag.qsb"})
    {
        if (QFile(QString::fromLatin1(shader)).size() < 100)
        {
            qCritical() << "Missing compiled QSB shader:" << shader;
            return 1;
        }
    }
    QJsonArray results;
    for (const auto *scene : {"GlassLab", "GlassIntegrationPreview"})
    {
        const auto name = QString::fromLatin1(scene);
        const bool integration = name == "GlassIntegrationPreview";
        const QSize fullSize = integration ? QSize(850, 480) : QSize(1080, 720);
        QQuickView view;
        QStringList warnings;
        QObject::connect(view.engine(), &QQmlEngine::warnings, &view,
                         [&warnings](const QList<QQmlError> &errors) {
                             for (const auto &error : errors)
                             {
                                 warnings.append(error.toString());
                             }
                         });
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.resize(fullSize);
        view.setSource(QUrl("qrc:/lab/" + name + ".qml"));
        auto *root = view.rootObject();
        if (!root || view.status() != QQuickView::Ready)
        {
            qCritical() << name << "could not load" << view.errors();
            return 1;
        }
        view.show();
        settle(700);
        const auto api = view.rendererInterface()->graphicsApi();
        if (api == QSGRendererInterface::Software ||
            api == QSGRendererInterface::Unknown)
        {
            qCritical() << "A real Qt Quick RHI shader backend is required:"
                        << api;
            return 1;
        }
        qInfo() << name << "Qt Quick graphics API:" << api;
        const auto shaders =
            root->findChildren<QQuickItem *>("meniscusRefractiveGlass");
        QQuickItem *composer = nullptr;
        for (auto *shader : shaders)
        {
            if (shader->width() > 300 &&
                (!composer || shader->mapToScene(QPointF()).y() >
                                  composer->mapToScene(QPointF()).y()))
            {
                composer = shader;
            }
        }
        if (shaders.size() < 2 || !composer)
        {
            qCritical()
                << "Expected the actual material and composer ShaderEffects";
            return 1;
        }
        const auto *toggle = integration ? "lensOn" : "glassEnabled";
        root->setProperty(toggle, false);
        settle();
        const auto baseline = view.grabWindow();
        root->setProperty(toggle, true);
        settle();
        const auto glass = view.grabWindow();
        if (baseline.isNull() || glass.size() != baseline.size())
        {
            return 1;
        }
        const qreal ratio = glass.width() / qreal(view.width());
        auto bounds = composer->mapRectToScene(composer->boundingRect());
        const QRect pixels(
            qRound(bounds.x() * ratio), qRound(bounds.y() * ratio),
            qRound(bounds.width() * ratio), qRound(bounds.height() * ratio));
        const auto region = pixels.intersected(glass.rect());
        int changed = 0;
        for (int y = region.top(); y <= region.bottom(); ++y)
        {
            for (int x = region.left(); x <= region.right(); ++x)
            {
                const auto a = baseline.pixel(x, y);
                const auto b = glass.pixel(x, y);
                changed += std::abs(qRed(a) - qRed(b)) +
                               std::abs(qGreen(a) - qGreen(b)) +
                               std::abs(qBlue(a) - qBlue(b)) >
                           9;
            }
        }
        if (changed < 100 ||
            !save(baseline, output + "/" + name + "-baseline.png") ||
            !save(glass, output + "/" + name + "-glass.png"))
        {
            qCritical() << "GPU composer optics did not affect real pixels:"
                        << changed;
            return 1;
        }
        view.resize(integration ? QSize(640, 400) : QSize(760, 507));
        settle();
        bounds = composer->mapRectToScene(composer->boundingRect());
        const QRectF viewport(QPointF(), QSizeF(view.size()));
        if (!viewport.adjusted(-1, -1, 1, 1).contains(bounds) ||
            bounds.width() <= 0 || bounds.height() <= 0 ||
            !save(view.grabWindow(), output + "/" + name + "-resized.png"))
        {
            qCritical() << "Resized composer escaped the viewport:" << bounds
                        << viewport;
            return 1;
        }
        view.resize(fullSize);
        settle();
        if (!save(view.grabWindow(), output + "/" + name + "-restored.png"))
        {
            return 1;
        }
        // Also instantiate the compatibility facade and reusable panel with the
        // required source supplied, instead of merely parsing their filenames.
        for (const auto *componentName : {"GlassPane", "LiquidGlassPanel"})
        {
            QQmlComponent component(
                view.engine(),
                QUrl("qrc:/lab/" + QString::fromLatin1(componentName) +
                     ".qml"));
            std::unique_ptr<QObject> instance(
                component.createWithInitialProperties(
                    {{"backgroundLayer", QVariant::fromValue(root)}}));
            if (!instance || component.isError())
            {
                qCritical() << component.errors();
                return 1;
            }
        }
        if (!warnings.isEmpty())
        {
            qCritical() << "QML/component warnings:" << warnings;
            return 1;
        }
        results.append(QJsonObject{{"scene", name},
                                   {"graphicsApi", int(api)},
                                   {"composerChangedPixels", changed},
                                   {"shaderEffects", shaders.size()},
                                   {"resizedComposerWidth", bounds.width()},
                                   {"resizedComposerHeight", bounds.height()}});
        view.close();
    }
    QFile measurements(output + "/gpu-checks.json");
    if (!measurements.open(QIODevice::WriteOnly) ||
        measurements.write(QJsonDocument(results).toJson()) <= 0)
    {
        return 1;
    }
    qInfo() << "QML component loading, real QSB optics and resized composer "
               "checks passed";
    return 0;
}
