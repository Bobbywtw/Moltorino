// SPDX-License-Identifier: MIT
// Standalone shader lab; deliberately does not load Moltorino private state.
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QSGRendererInterface>
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <algorithm>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Moltorino GPU Glass Lab"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption baseline(QStringList{QStringLiteral("baseline")},
                               QStringLiteral("Draw opaque baseline."));
    QCommandLineOption capture(QStringList{QStringLiteral("capture")},
                              QStringLiteral("Capture a real Qt Quick frame."),
                              QStringLiteral("file"));
    parser.addOption(baseline);
    parser.addOption(capture);
    parser.process(app);

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1080, 720);
    view.setTitle(QStringLiteral("Moltorino / Qt Quick GPU Glass Lab"));
    view.setColor(QColor("#192d40"));
    view.setSource(QUrl(QStringLiteral("qrc:/lab/GlassLab.qml")));

    QQuickItem *root = view.rootObject();
    if (view.status() == QQuickView::Error || !root) {
        qCritical() << "QML lab could not load";
        return 2;
    }
    root->setProperty("glassEnabled", !parser.isSet(baseline));

    QTimer resetMotion;
    resetMotion.setSingleShot(true);
    resetMotion.setInterval(95);
    QObject::connect(&resetMotion, &QTimer::timeout, root, [root] {
        root->setProperty("motionX", 0.0);
        root->setProperty("motionY", 0.0);
    });

    int lastX = view.x();
    int lastY = view.y();
    auto recordMove = [&] {
        const int dx = view.x() - lastX;
        const int dy = view.y() - lastY;
        lastX = view.x();
        lastY = view.y();
        if (dx || dy) {
            root->setProperty("motionX", std::clamp(-dx * 0.28, -7.0, 7.0));
            root->setProperty("motionY", std::clamp(-dy * 0.28, -7.0, 7.0));
            resetMotion.start();
        }
    };
    QObject::connect(&view, &QWindow::xChanged, root, recordMove);
    QObject::connect(&view, &QWindow::yChanged, root, recordMove);

    view.show();
    qInfo() << "Qt Quick graphics API:" << view.rendererInterface()->graphicsApi();

    if (parser.isSet(capture)) {
        QString filename = parser.value(capture);
        QTimer::singleShot(2000, &app, [&view, &app, filename] {
            auto image = view.grabWindow();
            if (image.isNull()) {
                qCritical() << "Could not capture the Qt Quick window";
                app.exit(3);
                return;
            }
            QDir().mkpath(QFileInfo(filename).absolutePath());
            if (!image.save(filename, "PNG")) {
                qCritical() << "Could not write screenshot:" << filename;
                app.exit(4);
                return;
            }
            qInfo() << "Saved real rendering screenshot:" << filename;
            app.exit(0);
        });
    }
    return app.exec();
}
