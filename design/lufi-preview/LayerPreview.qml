import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: 480
    height: 640
    minimumWidth: 320
    minimumHeight: 420
    title: "Lufi — Animation Preview"
    color: "#292a27"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                id: cat
                width: 522
                height: 760
                anchors.centerIn: parent
                scale: Math.min(parent.width / width, parent.height / height)

                Image {
                    source: "assets/Lufi_Loaf_Feet_Back.svg"
                }

                Item {
                    anchors.fill: parent
                    transform: Scale {
                        id: breathing
                        origin.x: 262
                        origin.y: 694
                        yScale: 1
                    }

                    Repeater {
                        model: ["Head", "Collar", "Mouth", "Face_Front", "Nose"]
                        Image {
                            required property string modelData
                            source: "assets/Lufi_Loaf_" + modelData + ".svg"
                            width: implicitWidth
                            height: implicitHeight
                        }
                    }

                    Image {
                        id: eyes
                        source: "assets/Lufi_Loaf_Eyes_Sad.svg"
                        transform: Scale {
                            id: eyelids
                            origin.x: 262
                            origin.y: 279
                            yScale: 1
                        }
                    }
                }

                Image {
                    source: "assets/Lufi_Loaf_Feet_Front.svg"
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16

            CheckBox {
                id: automatic
                text: "Automatic blinking"
                checked: true
                palette.windowText: "#f1efe8"
            }

            Button {
                text: "Blink"
                enabled: !blink.running
                onClicked: blink.start()
            }
        }
    }

    SequentialAnimation {
        running: true
        loops: Animation.Infinite
        NumberAnimation {
            target: breathing
            property: "yScale"
            from: 1
            to: 1.025
            duration: 1800
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: breathing
            property: "yScale"
            from: 1.025
            to: 1
            duration: 2400
            easing.type: Easing.InOutSine
        }
    }

    Timer {
        id: blinkTimer
        interval: 2500 + Math.random() * 3500
        running: automatic.checked && !blink.running
        onTriggered: blink.start()
    }

    SequentialAnimation {
        id: blink
        onFinished: blinkTimer.interval = 2500 + Math.random() * 3500
        NumberAnimation {
            target: eyelids
            property: "yScale"
            to: 0.06
            duration: 90
            easing.type: Easing.InQuad
        }
        PauseAnimation { duration: 90 }
        NumberAnimation {
            target: eyelids
            property: "yScale"
            to: 1
            duration: 150
            easing.type: Easing.OutQuad
        }
    }
}
