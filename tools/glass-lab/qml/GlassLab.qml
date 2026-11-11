import QtQuick 2.15

Item {
    id: root
    width: 1080
    height: 720
    objectName: "MoltorinoGpuGlassLab"

    property bool glassEnabled: true
    property real refractionStrength: 1.35
    property real vibrancy: 1.45
    property real gloss: 2.35
    property real chromaticDispersion: 0.65
    property real motionX: 0.0
    property real motionY: 0.0

    Behavior on motionX { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on motionY { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

    // Keep the entire test scene at one stable render resolution. During
    // continuous outer-window resizing only this native scene-graph transform
    // changes: no sourceRect or FBO reallocation of the glass composer.
    Item {
        id: stage
        width: 1080
        height: 720
        transformOrigin: Item.TopLeft
        scale: Math.min(root.width / width, root.height / height)
        x: (root.width - width * scale) / 2
        y: (root.height - height * scale) / 2

    // A high-contrast engineering backdrop (not a blue striped wallpaper):
    // optically recognizable details, fine lines, and mixed neutral geometry.
    // No part of glassForeground belongs to this capture layer.
    Item {
        id: appOwnedBackdrop
        anchors.fill: parent

        Rectangle { anchors.fill: parent; color: "#18191a" }

        Canvas {
            id: grid
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.lineWidth = 1
                for (let x = 0; x < width; x += 24) {
                    const major = x % 120 === 0
                    ctx.strokeStyle = major ? "rgba(232,232,232,0.28)"
                                            : "rgba(230,230,230,0.09)"
                    ctx.beginPath()
                    ctx.moveTo(x + 0.5, 0)
                    ctx.lineTo(x + 0.5, height)
                    ctx.stroke()
                }
                for (let y = 0; y < height; y += 24) {
                    const major = y % 120 === 0
                    ctx.strokeStyle = major ? "rgba(232,232,232,0.26)"
                                            : "rgba(230,230,230,0.08)"
                    ctx.beginPath()
                    ctx.moveTo(0, y + 0.5)
                    ctx.lineTo(width, y + 0.5)
                    ctx.stroke()
                }
                // Distinct reference geometry makes lens deformation obvious.
                ctx.lineWidth = 3
                ctx.strokeStyle = "rgba(236,229,218,0.82)"
                ctx.beginPath()
                ctx.arc(width * 0.53, height * 0.46, 126, 0, Math.PI * 2)
                ctx.stroke()
                ctx.lineWidth = 1
                ctx.strokeStyle = "rgba(240,235,228,0.45)"
                ctx.beginPath()
                ctx.arc(width * 0.53, height * 0.46, 161, 0, Math.PI * 2)
                ctx.stroke()
                ctx.strokeStyle = "rgba(243,237,228,0.72)"
                ctx.beginPath()
                ctx.moveTo(0, 335.5)
                ctx.lineTo(width, 335.5)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(700.5, 0)
                ctx.lineTo(700.5, height)
                ctx.stroke()
            }
        }

        // Accent details belong to the backdrop, never the glass color.
        Rectangle {
            x: 125; y: 246; width: 120; height: 120
            radius: 60; color: "#c3a178"; opacity: 0.64
        }
        Rectangle {
            x: 641; y: 271; width: 110; height: 110
            radius: 55; color: "#8caaa2"; opacity: 0.72
        }
        Rectangle {
            x: 810; y: 455; width: 125; height: 61
            radius: 4; color: "#d8d2c8"; opacity: 0.86
        }
        Rectangle {
            x: 844; y: 472; width: 56; height: 28
            radius: 1; color: "#222322"
        }

        Text {
            x: 95; y: 174
            text: "MATERIAL / 01"
            color: "#ecebe5"
            font.family: "Segoe UI"
            font.pixelSize: 54
            font.bold: true
            opacity: 0.78
        }
        Text {
            x: 160; y: 314
            text: "REFRACTION"
            color: "#f6f2e9"
            font.family: "Segoe UI"
            font.pixelSize: 51
            font.bold: true
            opacity: 0.88
        }
        Text {
            x: 605; y: 404
            text: "R  /  0447"
            color: "#f2ede5"
            font.family: "Consolas"
            font.pixelSize: 38
            opacity: 0.78
        }
        Text {
            x: 101; y: 595
            text: "COLOR   ◯   DETAIL  /  92"
            color: "#e2ded5"
            font.family: "Consolas"
            font.pixelSize: 34
            opacity: 0.78
        }
        Text {
            x: 66; y: 688
            text: "NON-RECURSIVE / OWNED BACKDROP / 1080 × 720"
            font.family: "Consolas"
            color: "#aeb1b1"
            font.pixelSize: 11
        }
    }

    Item {
        id: glassForeground
        anchors.fill: parent

        // Separate header and refractive controls.
        LiquidGlassPanel {
            id: headerLens
            x: 43; y: 28; width: stage.width - 86; height: 74
            backgroundLayer: appOwnedBackdrop
            active: root.glassEnabled
            refraction: root.refractionStrength
            vibrancy: root.vibrancy
            edgeLight: root.gloss
            dispersion: root.chromaticDispersion
            motionX: root.motionX
            motionY: root.motionY
        }
        Text {
            x: 72; y: 47
            text: "MOLTORINO"
            color: "#f8f7f2"
            font.family: "Segoe UI"
            font.bold: true
            font.pixelSize: 20
        }
        Text {
            x: 228; y: 53
            text: "GPU optics  /  signed-distance meniscus  /  build 06"
            color: "#e6e6e4"
            font.family: "Segoe UI"
            font.pixelSize: 14
        }

        // Lens toggle intentionally controls only material passes: same
        // background and geometry in both states, no fake comparison.
        Rectangle {
            x: stage.width - 215; y: 44; width: 148; height: 38
            radius: 19
            color: root.glassEnabled ? "#ddddda" : "#3e3e40"
            border.color: root.glassEnabled ? "#ffffff" : "#777777"
            border.width: 1
            Text {
                anchors.centerIn: parent
                text: root.glassEnabled ? "LENS ON" : "LENS OFF"
                color: root.glassEnabled ? "#222222" : "#eeeeee"
                font.bold: true
                font.family: "Segoe UI"
                font.pixelSize: 12
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.glassEnabled = !root.glassEnabled
            }
        }

        Row {
            x: 59; y: 118; spacing: 10

            Rectangle {
                width: 210; height: 36; radius: 10
                color: "#252526"; border.color: "#646465"
                Text {
                    anchors.centerIn: parent
                    text: "REFRACTION   " + root.refractionStrength.toFixed(2) + "   ↗"
                    color: "#e8e7e5"; font.pixelSize: 12; font.family: "Segoe UI"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.refractionStrength =
                        root.refractionStrength > 1.6 ? 0.65 : root.refractionStrength + 0.25
                }
            }
            Rectangle {
                width: 194; height: 36; radius: 10
                color: "#252526"; border.color: "#646465"
                Text {
                    anchors.centerIn: parent
                    text: "VIBRANCY   " + root.vibrancy.toFixed(2) + "   ↗"
                    color: "#e8e7e5"; font.pixelSize: 12; font.family: "Segoe UI"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.vibrancy =
                        root.vibrancy >= 1.6 ? 1.0 : root.vibrancy + 0.15
                }
            }
            Rectangle {
                width: 185; height: 36; radius: 10
                color: "#252526"; border.color: "#646465"
                Text {
                    anchors.centerIn: parent
                    text: "EDGE LIGHT   " + root.gloss.toFixed(2) + "   ↗"
                    color: "#e8e7e5"; font.pixelSize: 12; font.family: "Segoe UI"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.gloss = root.gloss > 2.6 ? 0.7 : root.gloss + 0.35
                }
            }
            Rectangle {
                width: 186; height: 36; radius: 10
                color: "#252526"; border.color: "#646465"
                Text {
                    anchors.centerIn: parent
                    text: "DISPERSION   " + root.chromaticDispersion.toFixed(2) + " px  ↗"
                    color: "#e8e7e5"; font.pixelSize: 12; font.family: "Segoe UI"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.chromaticDispersion =
                        root.chromaticDispersion >= 1.20 ? 0.0
                                                       : root.chromaticDispersion + 0.20
                }
            }
        }

        Text {
            x: 752; y: 211
            text: "OPTICAL TEST SURFACE"
            color: "#bcbdbb"
            font.family: "Segoe UI"
            font.pixelSize: 12
            font.letterSpacing: 2.4
        }
        LiquidGlassPanel {
            id: materialLens
            x: 95; y: 235; width: 614; height: 210
            backgroundLayer: appOwnedBackdrop
            active: root.glassEnabled
            refraction: root.refractionStrength
            vibrancy: root.vibrancy
            edgeLight: root.gloss
            dispersion: root.chromaticDispersion
            motionX: root.motionX
            motionY: root.motionY
        }
        // Chat text and badges must be drawn AFTER the shader, as separate
        // scene-graph nodes. The large REFRACTION lettering belongs to the
        // background and bends intentionally; this overlay never bends.
        Item {
            id: unwarpedForeground
            x: materialLens.x
            y: materialLens.y
            width: materialLens.width
            height: materialLens.height
            z: 10

            Text {
                x: 31; y: 21
                text: "01  /  MENISCUS  •  TEXT ISOLATION"
                font.family: "Segoe UI"
                font.pixelSize: 12
                font.bold: true
                color: "#ffffff"
            }
            Row {
                x: 31; y: 66; spacing: 8
                Rectangle {
                    width: 21; height: 21; radius: 5
                    color: "#c6e7c9"
                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        color: "#243729"
                        font.pixelSize: 15
                        font.bold: true
                    }
                }
                Text {
                    text: "chat-user"
                    font.family: "Segoe UI"
                    font.pixelSize: 16
                    font.bold: true
                    color: "#e4c1ff"
                }
                Text {
                    text: " : Text and badges stay sharp."
                    font.family: "Segoe UI"
                    font.pixelSize: 16
                    color: "#ffffff"
                }
            }
            Text {
                x: 31; y: 165
                text: "Look for bending in the BACKDROP grid, lettering and circular lines"
                font.family: "Segoe UI"
                font.pixelSize: 13
                color: "#f5f5f0"
            }
        }

        Text {
            x: 73; y: stage.height - 170
            text: "COMPOSER   /   LIGHT CATCH"
            font.family: "Segoe UI"
            font.pixelSize: 12
            color: "#bcbdbb"
            font.letterSpacing: 1.7
        }

        // The composer is an independent material. Its bright ridge floats
        // ABOVE it rather than being a dark border painted on the pane.
        LiquidGlassPanel {
            id: composerLens
            x: 61; y: stage.height - 122
            width: stage.width - 122; height: 74
            backgroundLayer: appOwnedBackdrop
            active: root.glassEnabled
            refraction: root.refractionStrength
            vibrancy: root.vibrancy
            edgeLight: root.gloss
            dispersion: root.chromaticDispersion
            motionX: root.motionX
            motionY: root.motionY
        }
        Rectangle {
            id: composerRidge
            x: composerLens.x + 18; y: composerLens.y - 8
            width: composerLens.width - 36; height: 2
            radius: 1
            opacity: root.glassEnabled ? 0.88 : 0.0
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#555554" }
                GradientStop { position: 0.36; color: "#dededa" }
                GradientStop { position: 0.73; color: "#5c5c5b" }
                GradientStop { position: 1.0; color: "#333333" }
            }
        }
        Rectangle {
            x: composerRidge.x +
               Math.max(0, Math.min(1, composerLens.cursorUV.x)) *
               Math.max(0, composerRidge.width - 94)
            y: composerRidge.y - 1
            width: 94; height: 4; radius: 2
            color: "#ffffff"
            opacity: root.glassEnabled &&
                     composerLens.cursorUV.x >= 0 &&
                     composerLens.cursorUV.x <= 1 ? 0.85 : 0.0
            Behavior on x { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        }
        Text {
            x: composerLens.x + 25; y: composerLens.y + 21
            text: "Send message as"
            color: "#fafaf7"
            font.family: "Segoe UI"
            font.pixelSize: 18
        }
        Text {
            x: composerLens.x + composerLens.width - 185
            y: composerLens.y + 27
            text: root.glassEnabled ? "TRANSMISSIVE" : "FLAT / BASELINE"
            color: "#e6e5e0"
            font.family: "Consolas"
            font.pixelSize: 12
            font.bold: true
        }
    }
    }
}
