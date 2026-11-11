import QtQuick 2.15

Item {
    id: root
    width: 1080
    height: 720
    objectName: "MoltorinoGpuGlassLab"
    property bool glassEnabled: true
    property real refractionStrength: 1.0
    property real vibrancy: 1.30
    property real gloss: 0.85
    property real motionX: 0.0
    property real motionY: 0.0
    Behavior on motionX { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on motionY { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

    Item {
        id: appOwnedBackdrop
        anchors.fill: parent
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#192d40" }
                GradientStop { position: 0.5; color: "#2c3d4a" }
                GradientStop { position: 1.0; color: "#1b2535" }
            }
        }
        Repeater {
            model: 18
            Rectangle {
                x: -55 + index * 74
                y: -160
                width: 24
                height: 1090
                rotation: -22
                color: index % 2 === 0 ? "#a6d3df" : "#e2a9d6"
                opacity: index % 2 === 0 ? 0.22 : 0.16
            }
        }
        Repeater {
            model: 11
            Rectangle {
                x: 70 + (index % 4) * 260
                y: 140 + Math.floor(index / 4) * 150
                width: 192
                height: 92
                radius: 15
                color: index % 3 === 0 ? "#728ca1" : "#627b77"
                opacity: 0.36
            }
        }
        Repeater {
            model: 11
            Text {
                x: 88 + (index % 4) * 260
                y: 156 + Math.floor(index / 4) * 150
                text: index % 2 ? "background • texture" : "Moltorino / active chat"
                font.pixelSize: 14
                font.family: "Segoe UI"
                color: "#c1dbec"
                opacity: 0.55
            }
        }
    }

    Item {
        id: glassForeground
        anchors.fill: parent
        GlassPane {
            x: 38; y: 30; width: root.width - 76; height: 64
            backgroundLayer: appOwnedBackdrop
            active: root.glassEnabled
            strength: root.refractionStrength
            saturation: root.vibrancy
            gloss: root.gloss
            motionX: root.motionX
            motionY: root.motionY
        }

        Text {
            x: 64; y: 52
            text: "MOLTORINO"
            color: "#f5f7fd"
            font.family: "Segoe UI"
            font.bold: true
            font.pixelSize: 19
        }
        Text {
            x: 222; y: 54
            text: "Real-time .qsb refraction shader prototype"
            color: "#e1edf7"
            font.pixelSize: 16
            font.family: "Segoe UI"
        }

        GlassPane {
            x: 86; y: 245; width: 438; height: 142
            backgroundLayer: appOwnedBackdrop
            active: root.glassEnabled
            strength: root.refractionStrength * 1.15
            saturation: root.vibrancy
            gloss: root.gloss
            motionX: root.motionX
            motionY: root.motionY
        }
        Text {
            x: 114; y: 268
            text: "LIVE GPU MATERIAL"
            color: "#fff"
            font.family: "Segoe UI"
            font.bold: true
            font.pixelSize: 13
        }
        Text {
            x: 114; y: 301
            text: root.glassEnabled ? "Lens displacement + vibrancy" : "Baseline: opaque Qt Rectangle"
            color: "#f6f9fc"
            font.family: "Segoe UI"
            font.pixelSize: 18
        }
        Text {
            x: 114; y: 344
            text: "Hover for specular response • drag window for parallax"
            color: "#d4e1e9"
            font.family: "Segoe UI"
            font.pixelSize: 12
        }

        GlassPane {
            x: 54; y: root.height - 121; width: root.width - 108; height: 70
            backgroundLayer: appOwnedBackdrop
            active: root.glassEnabled
            strength: root.refractionStrength
            saturation: root.vibrancy
            gloss: root.gloss
            motionX: root.motionX
            motionY: root.motionY
        }
        Text {
            x: 86; y: root.height - 99
            text: "Send message as"
            color: "#fff"
            font.family: "Segoe UI"
            font.pixelSize: 19
        }
        Text {
            x: root.width - 295; y: root.height - 99
            text: "GPU MATERIAL TEST"
            color: "#e2eafa"
            font.pixelSize: 12
            font.bold: true
        }
        Text {
            x: 57; y: root.height - 29
            text: "ISOLATED SHADER LAB  •  NOT CONNECTED TO CHAT OR DESKTOP CAPTURE"
            color: "#bcc9d8"
            font.family: "Segoe UI"
            font.pixelSize: 11
        }

        Rectangle {
            id: toggle
            x: root.width - 214
            y: 43
            width: 152
            height: 36
            radius: 18
            color: root.glassEnabled ? "#256c61" : "#525260"
            border.color: "#b5d6dc"
            Text {
                anchors.centerIn: parent
                text: root.glassEnabled ? "LENS ON" : "LENS OFF"
                color: "#ffffff"
                font.pixelSize: 13
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.glassEnabled = !root.glassEnabled
            }
        }

        Row {
            x: 66; y: 112
            spacing: 22
            Text { text: "Strength " + root.refractionStrength.toFixed(2); color: "#fff"; font.pixelSize: 13 }
            Text { text: "Vibrancy " + root.vibrancy.toFixed(2); color: "#fff"; font.pixelSize: 13 }
            Text { text: "Gloss " + root.gloss.toFixed(2); color: "#fff"; font.pixelSize: 13 }
        }
    }
}
