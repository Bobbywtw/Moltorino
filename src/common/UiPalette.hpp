// SPDX-License-Identifier: MIT
#pragma once

#include <QColor>

#include <algorithm>

namespace chatterino {

// Application chrome is deliberately neutral. Message identity, emote, badge,
// paint and highlight colors continue to come from their existing providers.
struct UiPalette {
    QColor canvas;
    QColor surface;
    QColor raised;
    QColor border;
    QColor text;
    QColor muted;
    QColor focus;
};

inline const UiPalette &uiPalette(bool light = false)
{
    static const UiPalette dark{
        QColor(20, 20, 22),    QColor(30, 30, 33, 248),
        QColor(40, 40, 44),    QColor(255, 255, 255, 20),
        QColor(235, 235, 239), QColor(150, 150, 159),
        QColor(133, 133, 146),
    };
    static const UiPalette bright{
        QColor(244, 244, 246), QColor(251, 251, 252, 250),
        QColor(231, 231, 235), QColor(0, 0, 0, 24),
        QColor(34, 34, 39),    QColor(111, 111, 122),
        QColor(115, 115, 130),
    };
    return light ? bright : dark;
}

inline QColor uiMix(const QColor &from, const QColor &to, qreal progress)
{
    const auto t = std::clamp(progress, 0.0, 1.0);
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                            from.greenF() + (to.greenF() - from.greenF()) * t,
                            from.blueF() + (to.blueF() - from.blueF()) * t,
                            from.alphaF() + (to.alphaF() - from.alphaF()) * t);
}

}  // namespace chatterino
