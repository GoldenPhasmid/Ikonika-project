import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ikonika.Animation

ApplicationWindow
{
    id: window
    visible: true
    width: 700
    height: 850
    title: "Lufi — Animation Demo"
    color: light.checked ? "#eeeeeb" : "#292a27"
    palette.windowText: light.checked ? "#222222" : "#eeeeee"
    property real frame: 1
    property bool playing: playback.running && !playback.paused
    property bool blinking: false
    readonly property var clipInfo: AnimationSource.Clips[clip.currentIndex] || ({})
    readonly property int frameCount: clipInfo.frames || 1
    readonly property string clipId: clipInfo.id || ""
    function reloadAssets() { playback.stop(); window.frame = 1; AnimationSource.Reload(); clip.currentIndex = 0; playback.start() }
    Shortcut { sequence: "F1"; onActivated: { playback.stop(); window.frame = 1 } }
    Shortcut { sequence: "F2"; onActivated: { playback.stop(); window.frame = 24 } }
    Shortcut { sequence: "F3"; onActivated: { playback.stop(); window.frame = 48 } }
    Shortcut { sequence: "F4"; onActivated: { playback.stop(); window.frame = window.frameCount } }
    Shortcut { sequence: "F5"; onActivated: { playback.stop(); window.frame = 60 } }
    Shortcut { sequence: "F6"; onActivated: { playback.stop(); window.frame = 23 } }
    Shortcut { sequence: "F7"; onActivated: { playback.stop(); window.frame = 27 } }
    Shortcut { sequence: "F8"; onActivated: { playback.stop(); window.frame = 28 } }
    Shortcut { sequence: "F9"; onActivated: { playback.stop(); window.frame = 29 } }
    Shortcut { sequence: "F10"; onActivated: { playback.stop(); window.frame = 31 } }
    Shortcut { sequence: "F11"; onActivated: { playback.stop(); window.frame = 36 } }
    Shortcut { sequence: "F12"; onActivated: { playback.stop(); window.frame = 32 } }
    Shortcut { sequence: "A"; onActivated: { playback.stop(); window.frame = 13 } }
    Shortcut { sequence: "E"; onActivated: { playback.stop(); window.frame = 16 } }
    Shortcut { sequence: "T"; onActivated: { playback.stop(); window.frame = 20 } }
    Shortcut { sequence: "U"; onActivated: { playback.stop(); window.frame = 75 } }
    Shortcut { sequence: "Space"; onActivated: { if (window.playing) playback.pause(); else if (playback.paused) playback.resume(); else playback.start() } }
    Shortcut { sequence: "N"; onActivated: eyes.currentIndex = (eyes.currentIndex + 1) % eyes.count }

    Shortcut { sequence: "B"; onActivated: closed.checked = !closed.checked }
    Shortcut { sequence: "L"; onActivated: light.checked = !light.checked }
    Shortcut { sequence: "C"; onActivated: { playback.stop(); window.frame = 1; clip.currentIndex = (clip.currentIndex + 1) % clip.count; if (window.frameCount > 1) playback.start() } }
    Shortcut { sequence: "S"; onActivated: art.grabToImage(function(result) { result.saveToFile(AnimationSource.CaptureFile(window.clipId + "-frame-" + Math.floor(window.frame) + "-" + window.selectedEyes.replace(/ /g, "-"))) }) }
    readonly property string selectedEyes: blinking || closed.checked ? "Eyes Closed" : eyes.currentText
    Shortcut { sequence: "R"; onActivated: window.reloadAssets() }

    NumberAnimation on frame
    {
        id: playback
        from: 1
        to: window.frameCount + 1
        duration: 1000 * window.frameCount / (window.clipInfo.fps || 24)
        loops: Animation.Infinite
        running: true
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        Flow
        {
            Layout.fillWidth: true
            spacing: 8
            Button { text: window.playing ? "Pause" : "Play"; onClicked: { if (window.playing) playback.pause(); else if (playback.paused) playback.resume(); else playback.start() } }
            ComboBox { id: eyes; model: AnimationSource.Eyes }
            ComboBox { id: clip; model: AnimationSource.Clips; textRole: "name"; onActivated: { playback.stop(); window.frame = 1; if (window.frameCount > 1) playback.start() } }
            Button { text: "Reload"; onClicked: window.reloadAssets() }
            CheckBox { id: closed; text: "Closed" }
            Button { text: "Blink"; onClicked: { window.blinking = true; blinkTimer.restart() } }
        }
        Flow
        {
            Layout.fillWidth: true
            spacing: 8
            CheckBox { id: light; text: "Light background" }
            Label { text: "Frame " + Math.min(window.frameCount, Math.floor(window.frame)) + " / " + window.frameCount; color: light.checked ? "#222222" : "#ffffff" }
        }
        Label { Layout.fillWidth: true; visible: text.length > 0; text: AnimationSource.Error; color: "#ff8080"; wrapMode: Text.Wrap }
        Item
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Item
            {
                id: art
                width: window.clipInfo.width || 1
                height: window.clipInfo.height || 1
                anchors.centerIn: parent
                scale: Math.min(parent.width / width, parent.height / height)
                LufiAnimation
                {
                    anchors.fill: parent
                    animationSource: AnimationSource
                    frame: Math.floor(window.frame)
                    selectedEyes: window.selectedEyes
                    clipName: window.clipId
                }
            }
        }
        Slider
        {
            Layout.fillWidth: true
            from: 1
            to: window.frameCount
            stepSize: 1
            value: window.frame
            onMoved: { playback.stop(); window.frame = value }
        }
    }
    Timer { id: blinkTimer; interval: 160; onTriggered: window.blinking = false }
}
