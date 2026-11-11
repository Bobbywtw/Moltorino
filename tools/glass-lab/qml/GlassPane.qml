import QtQuick 2.15
// Backward-compatible facade for the earlier standalone test API.
LiquidGlassPanel {
    property real strength: 1.3
    property real saturation: 1.42
    property real gloss: 2.35
    property real chroma: 0.65
    refraction: strength
    vibrancy: saturation
    edgeLight: gloss
    dispersion: chroma
}
