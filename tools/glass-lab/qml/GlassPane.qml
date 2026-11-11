import QtQuick 2.15

Item {
    id: pane
    required property Item backgroundLayer
    property bool active: true
    property real strength: 1.3
    property real saturation: 1.42
    property real gloss: 2.35
    property real motionX: 0.0
    property real motionY: 0.0
    property point cursorUV: Qt.point(-2, -2)

    // UV displacement (up to 29px at maximum strength) and movement padding.
    readonly property real inset: 44
    readonly property int textureWidth: Math.max(1, Math.ceil(width + inset * 2))
    readonly property int textureHeight: Math.max(1, Math.ceil(height + inset * 2))

    ShaderEffectSource {
        id: isolatedBackdrop
        visible: false
        sourceItem: pane.backgroundLayer
        sourceRect: {
            const p = pane.mapToItem(pane.backgroundLayer, -pane.inset, -pane.inset)
            return Qt.rect(p.x, p.y, pane.textureWidth, pane.textureHeight)
        }
        textureSize: Qt.size(pane.textureWidth, pane.textureHeight)
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
        property real refractionStrength: pane.strength
        property real vibrancy: pane.saturation
        property real specularGloss: pane.gloss
        property size panelSize: Qt.size(pane.width, pane.height)
        property size sourceSize: Qt.size(pane.textureWidth, pane.textureHeight)
        property point pointerUV: pane.cursorUV
        property point parallaxPixels: Qt.point(pane.motionX, pane.motionY)
        property real sourceInset: pane.inset
        property real lensEnabled: pane.active ? 1.0 : 0.0

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
            pane.cursorUV = Qt.point(mouse.x / Math.max(1, pane.width),
                                     mouse.y / Math.max(1, pane.height))
        }
        onExited: pane.cursorUV = Qt.point(-2, -2)
    }
}
