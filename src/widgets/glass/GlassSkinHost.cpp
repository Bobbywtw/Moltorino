// SPDX-License-Identifier: MIT
#include "widgets/glass/GlassSkinHost.hpp"
#include <QCoreApplication>
#include <QPluginLoader>
#include <QDebug>
#include <QString>
namespace chatterino::glass {
namespace {
QString pluginPath() {
#if defined(Q_OS_WIN)
    const auto name = "/glass/moltorino-liquid-glass.dll";
#elif defined(Q_OS_MACOS)
    const auto name = "/glass/libmoltorino-liquid-glass.dylib";
#else
    const auto name = "/glass/libmoltorino-liquid-glass.so";
#endif
    return QCoreApplication::applicationDirPath() + QString::fromLatin1(name);
}
struct Loaded {
    QPluginLoader loader;
    GlassSkin *skin = nullptr;
    Loaded() : loader(pluginPath()) {
        QObject *instance = loader.instance();
        skin = qobject_cast<GlassSkin *>(instance);
        if (skin) qInfo() << "Moltorino native glass plugin loaded:" << pluginPath();
        else qWarning() << "Moltorino native glass plugin unavailable:" << loader.errorString();
    }
};
}
bool paint(QPainter &painter, const QRectF &rect, Surface surface,
           qreal emphasis, qreal scale) {
    static Loaded loaded; // Never load DLLs from Lua plugin or PATH directories.
    if (!loaded.skin || rect.isEmpty()) return false;
    loaded.skin->paint(painter, rect, surface, emphasis, scale);
    return true;
}
}
