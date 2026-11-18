import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window
    visible: true
    width: 390
    height: 620
    minimumWidth: 280
    minimumHeight: 440
    title: "Lufi — Sprite Preview"
    color: "#292a27"
    property int frameIndex: 0
    property real playbackStarted: Date.now()

    Image {
        anchors.centerIn: parent
        width: Math.min(256, window.width - 48)
        height: width * 400 / 256
        source: "assets/paw-lick/sheet.png"
        sourceClipRect: Qt.rect((window.frameIndex % 8) * 256,
                               Math.floor(window.frameIndex / 8) * 400,
                               256, 400)
        smooth: true
    }

    Timer {
        interval: 16
        repeat: true
        running: window.visible
        onTriggered: {
            const elapsed = (Date.now() - window.playbackStarted) % 8000
            window.frameIndex = elapsed < 3000 ? Math.min(71, Math.floor(elapsed * 24 / 1000)) : 71
        }
    }
}

