import QtQuick

Item {
    id: chrome
    property url backdropUrl
    property bool opticsEnabled: true
    property bool systemBackdrop: false
    NumberAnimation on opacity { from: 0.88; to: 1.0; duration: 120; easing.type: Easing.OutCubic }
    // The same GPU material as Build #5. The native widgets above this scene
    // own all text, icons, emotes, hit testing, focus, accessibility and actions.
    Item {
        id: backdrop
        anchors.fill: parent
        // DWM supplies real desktop diffusion in this mode. Keep this source
        // available to the unchanged optical shader, but don't cover DWM.
        visible: !chrome.systemBackdrop
        Rectangle { anchors.fill: parent; color: "#171819" }
        Image { anchors.fill: parent; source: chrome.backdropUrl; cache: false }
    }
    Repeater {
        model: chromePanels
        delegate: Item {
            id: surface
            required property rect bounds
            required property string kind
            required property real emphasis
            required property bool hovered
            required property bool pressed
            required property bool disabled
            required property point pointer
            x: bounds.x
            y: bounds.y
            property real pressOffset: pressed ? 0.7 : 0
            transform: Translate { y: surface.pressOffset }
            Behavior on pressOffset { NumberAnimation { duration: 70; easing.type: Easing.OutCubic } }
            width: bounds.width
            height: bounds.height
            visible: width > 1 && height > 1
            z: kind === "window" || kind === "toolbar" ? 0 : kind === "button" || kind === "field" ? 2 : 1
            property real engagement: disabled ? 0 : pressed ? 1.0 : kind === "button" ? (hovered ? 0.65 : 0) : Math.max(emphasis, hovered ? 0.65 : 0.0)
            Behavior on engagement { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
            opacity: (chrome.systemBackdrop ? (kind === "window" ? 0.16 : kind === "field" ? 0.38 : 0.28) : 1.0)
                     * (disabled ? (kind === "button" ? 0.12 : 0.38) : kind === "button" ? (0.32 + engagement * 0.68) : 1.0)
            Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
            LiquidGlassPanel {
                anchors.fill: parent
                backgroundLayer: backdrop
                sourceOrigin: Qt.point(surface.x, surface.y + surface.pressOffset)
                active: chrome.opticsEnabled
                refraction: 1.35 + surface.engagement * 0.12
                vibrancy: 1.45
                edgeLight: 2.35 + surface.engagement * 0.3
                dispersion: 0.65
                motionX: surface.hovered ? (surface.pointer.x - 0.5) * 0.65 : 0
                motionY: surface.hovered ? (surface.pointer.y - 0.5) * 0.65 : 0
                cursorUV: surface.pointer
            }
        }
    }
}
