import QtQuick 2.15

Item {
    id: root
    width: 850
    height: 480
    property bool lensOn: true

    // Stable logical stage ensures its ShaderEffectSource FBO need not resize
    // on each tick of QWidget::createWindowContainer window resizing.
    Item {
        id: stage
        width: 850
        height: 480
        transformOrigin: Item.TopLeft
        scale: Math.min(root.width / width, root.height / height)
        x: (root.width - width * scale) / 2
        y: (root.height - height * scale) / 2

        Item {
            id: ownedBackdrop
            anchors.fill: parent
            Rectangle { anchors.fill: parent; color: "#18191a" }
            Canvas {
                anchors.fill: parent
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.lineWidth = 1
                    for (let x = 0; x < width; x += 22) {
                        ctx.strokeStyle = x % 110 === 0 ?
                            "rgba(240,240,240,0.3)" : "rgba(240,240,240,0.09)"
                        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
                    }
                    for (let y = 0; y < height; y += 22) {
                        ctx.strokeStyle = y % 110 === 0 ?
                            "rgba(240,240,240,0.3)" : "rgba(240,240,240,0.09)"
                        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                    }
                    ctx.strokeStyle = "rgba(240,240,240,0.7)"
                    ctx.lineWidth = 2
                    ctx.beginPath()
                    ctx.arc(445, 231, 112, 0, Math.PI * 2)
                    ctx.stroke()
                }
            }
            Text {
                x: 95; y: 155; text: "REFRACTED BACKDROP"
                color: "#dfddd8"; font.family: "Segoe UI"
                font.pixelSize: 38; font.bold: true
            }
            Rectangle { x: 422; y: 172; width: 95; height: 95; radius: 48; color: "#c9ac83"; opacity: 0.66 }
        }

        LiquidGlassPanel {
            id: glass
            x: 45; y: 126; width: 760; height: 179
            backgroundLayer: ownedBackdrop
            active: root.lensOn
            refraction: 1.35
            vibrancy: 1.45
            edgeLight: 2.35
            dispersion: 0.65
        }
        Text {
            x: glass.x + 24; y: glass.y + 20
            text: "Qt Quick GPU glass integrated with Moltorino"
            color: "#fff"; font.family: "Segoe UI"
            font.pixelSize: 17; font.bold: true
        }
        Row {
            x: glass.x + 25; y: glass.y + 73; spacing: 10
            Rectangle {
                width: 25; height: 25; radius: 5; color: "#c4e5cb"
                Text { anchors.centerIn: parent; text: "✓"; color: "#214229"; font.pixelSize: 17 }
            }
            Text { text: "sample-user"; color: "#e3b8fa"; font.family: "Segoe UI"; font.pixelSize: 17; font.bold: true }
            Text { text: ": the chat foreground stays perfectly sharp"; color: "#fff"; font.family: "Segoe UI"; font.pixelSize: 17 }
        }
        LiquidGlassPanel {
            id: composer
            x: 45; y: 359; width: 760; height: 65
            backgroundLayer: ownedBackdrop
            active: root.lensOn
            refraction: 1.35; vibrancy: 1.45
            edgeLight: 2.35; dispersion: 0.65
        }
        Text {
            x: composer.x + 20; y: composer.y + 20
            text: "Send message as (simulated)"
            color: "#fff"; font.family: "Segoe UI"; font.pixelSize: 17
        }
        Text {
            x: 45; y: 27
            text: "MOLTORINO / GPU QUICK PREVIEW"
            color: "#fff"; font.family: "Segoe UI"; font.pixelSize: 18; font.bold: true
        }
        Rectangle {
            x: 655; y: 18; width: 150; height: 36; radius: 18
            color: root.lensOn ? "#dddcd8" : "#363636"
            Text {
                anchors.centerIn: parent; text: root.lensOn ? "LENS ON" : "LENS OFF"
                color: root.lensOn ? "#151515" : "#fafafa"
                font.family: "Segoe UI"; font.pixelSize: 12; font.bold: true
            }
            MouseArea { anchors.fill: parent; onClicked: root.lensOn = !root.lensOn }
        }
        Text {
            x: 46; y: 452
            text: "NO REAL CHAT • NO DESKTOP CAPTURE • SAFE NATIVE QT CHILD WINDOW"
            color: "#aaaaaa"; font.family: "Segoe UI"; font.pixelSize: 11
        }
    }
}
