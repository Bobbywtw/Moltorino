#pragma once

#include <QColor>
#include <QString>

namespace chatterino {

struct SettingsTheme {
    QColor background;
    QColor surface;
    QColor raisedSurface;
    QColor text;
    QColor mutedText;
    QColor accent;
    QColor selectionSurface;
    QString interfaceFontFamily;
    int interfaceFontSize;
};

inline const SettingsTheme &settingsTheme()
{
    static const SettingsTheme value{
        .background = QColor(QStringLiteral("#141416")),
        .surface = QColor(QStringLiteral("#1e1e21")),
        .raisedSurface = QColor(QStringLiteral("#28282c")),
        .text = QColor(QStringLiteral("#ebebef")),
        .mutedText = QColor(QStringLiteral("#96969f")),
        .accent = QColor(QStringLiteral("#b1b1bc")),
        .selectionSurface = QColor(QStringLiteral("#303034")),
        .interfaceFontFamily = QStringLiteral("Segoe UI"),
        .interfaceFontSize = 10,
    };
    return value;
}

}
