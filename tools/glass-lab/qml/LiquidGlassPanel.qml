import QtQuick 2.15

Item {
    id: panel
    required property Item backgroundLayer
    property bool active: true
    property real refraction: 1.3
    property real vibrancy: 1.42
    property real edgeLight: 2.35
    property real dispersion: 0.65
    property real motionX: 0.0
    property real motionY: 0.0
    property point cursorUV: Qt.point(-2, -2)

    // Refraction (up to 16.2px at max strength) + parallax sampling padding.
    readonly property real inset: 44
    readonly property int textureWidth: Math.max(1, Math.ceil(width + inset * 2))
    readonly property int textureHeight: Math.max(1, Math.ceil(height + inset * 2))

    ShaderEffectSource {
        id: isolatedBackdrop
        visible: false
        sourceItem: panel.backgroundLayer
        sourceRect: {
            const p = panel.mapToItem(panel.backgroundLayer, -panel.inset, -panel.inset)
            return Qt.rect(p.x, p.y, panel.textureWidth, panel.textureHeight)
        }
        // Let Qt allocate at native device pixel ratio. While an outer
        // window is resizing, the material's logical size remains stable.
        smooth: true
        live: true
        hideSource: false
        recursive: false
    }

    ShaderEffect {
        anchors.fill: parent
        objectName: "meniscusRefractiveGlass"
        vertexShader: "qrc:/shaders/glass.vert.qsb"
        fragmentShader: "qrc:/shaders/glass.frag.qsb"
        blending: true
        property var source: isolatedBackdrop
        property real refractionStrength: panel.refraction
        property real vibrancy: panel.vibrancy
        property real specularGloss: panel.edgeLight
        property real dispersionPixels: panel.dispersion
        property size panelSize: Qt.size(panel.width, panel.height)
        property size sourceSize: Qt.size(panel.textureWidth, panel.textureHeight)
        property point pointerUV: panel.cursorUV
        property point parallaxPixels: Qt.point(panel.motionX, panel.motionY)
        property real sourceInset: panel.inset
        property real lensEnabled: panel.active ? 1.0 : 0.0

        onStatusChanged: {
            if (status === ShaderEffect.Error)
                console.error("GLASS_SHADEREFFECT_ERROR:", log)
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        onPositionChanged: function(mouse) {
            panel.cursorUV = Qt.point(mouse.x / Math.max(1, panel.width),
                                     mouse.y / Math.max(1, panel.height))
        }
        onExited: panel.cursorUV = Qt.point(-2, -2)
    }
}
