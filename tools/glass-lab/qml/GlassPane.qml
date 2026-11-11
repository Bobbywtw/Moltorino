import QtQuick 2.15

Item {
    id: pane
    required property Item backgroundLayer
    property bool active: true
    property real strength: 1.0
    property real saturation: 1.28
    property real gloss: 0.9
    property real motionX: 0.0
    property real motionY: 0.0
    property point cursorUV: Qt.point(-2, -2)
    readonly property real inset: 16
    readonly property int textureWidth: Math.max(1, Math.ceil(width + inset * 2))
    readonly property int textureHeight: Math.max(1, Math.ceil(height + inset * 2))

    ShaderEffectSource {
        id: backdrop
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

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: "#282d35"
        border.color: "#59636e"
        border.width: 1
        visible: !pane.active
    }

    ShaderEffect {
        anchors.fill: parent
        visible: pane.active
        objectName: "realTextureGlass"
        vertexShader: "qrc:/shaders/glass.vert.qsb"
        fragmentShader: "qrc:/shaders/glass.frag.qsb"
        blending: true
        property var source: backdrop
        property real refractionStrength: pane.strength
        property real vibrancy: pane.saturation
        property real specularGloss: pane.gloss
        property size panelSize: Qt.size(pane.width, pane.height)
        property size sourceSize: Qt.size(pane.textureWidth, pane.textureHeight)
        property point pointerUV: pane.cursorUV
        property point parallaxPixels: Qt.point(pane.motionX, pane.motionY)
        property real sourceInset: pane.inset

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
