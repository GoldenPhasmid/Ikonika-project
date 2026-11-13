import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../FormsMarkup.js" as FormsMarkup

GridLayout
{
    id: root

    property var cardModel: []
    property var verbGroups: []
    property var stateColours: []
    property var cellOpacities: []
    property var filledCells: []
    property var keyHints: []
    property bool interactive: true
    property bool styledText: false
    property bool voiceColumn: false
    property int playingIndex: -1
    property int cellHeight: AppMetrics.optionCellHeight

    property bool popArmed: false
    property bool shakeArmed: false

    signal optionClicked(int index)

    function armFeedback(correct)
    {
        root.popArmed = correct
        root.shakeArmed = !correct

        Qt.callLater(function()
        {
            root.popArmed = false
            root.shakeArmed = false
        })
    }

    columns: 2
    rowSpacing: AppMetrics.gapCompact
    columnSpacing: AppMetrics.gapCompact

    Repeater
    {
        model: root.cardModel

        delegate: Button
        {
            id: cell

            required property string modelData
            required property int index

            readonly property color stateColour: cell.index < root.stateColours.length ? root.stateColours[cell.index] : AppTheme.colourBorder
            readonly property bool isCorrect: Qt.colorEqual(cell.stateColour, AppTheme.colourCorrect)
            readonly property bool isWrong: Qt.colorEqual(cell.stateColour, AppTheme.colourWrong)
            readonly property bool isSelected: Qt.colorEqual(cell.stateColour, AppTheme.colourAccent)
            readonly property bool isAnswered: cell.isCorrect || cell.isWrong
            readonly property string keyHint: cell.index < root.keyHints.length ? root.keyHints[cell.index] : ""
            readonly property bool isVoice: root.voiceColumn && cell.index % 2 === 0 && !cell.isCorrect
            readonly property bool isFilled: cell.index < root.filledCells.length && !!root.filledCells[cell.index]

            property real shakeOffset: 0

            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.preferredHeight: root.cellHeight

            leftPadding: AppMetrics.gapMedium + (cell.keyHint.length > 0 ? hintBox.implicitWidth + AppMetrics.gapCompact : 0)
            rightPadding: AppMetrics.gapMedium + AppMetrics.keyHintSize

            hoverEnabled: true
            focusPolicy: Qt.NoFocus
            enabled: root.interactive
            opacity: cell.index < root.cellOpacities.length ? root.cellOpacities[cell.index] : 1.0

            text: cell.modelData.replace(/<[^>]+>/g, "")
            Accessible.name: cell.keyHint.length > 0 ? qsTr("%1, key %2").arg(cell.text).arg(cell.keyHint) : cell.text

            transform: Translate { x: cell.shakeOffset }

            onIsCorrectChanged: if (cell.isCorrect && root.popArmed) pop.restart()
            onIsWrongChanged: if (cell.isWrong && root.shakeArmed) shake.restart()

            onClicked: root.optionClicked(cell.index)

            Behavior on opacity { NumberAnimation { duration: AppMetrics.motionBase } }

            SequentialAnimation
            {
                id: pop

                NumberAnimation { target: cell; property: "scale"; to: 0.96; duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
                NumberAnimation { target: cell; property: "scale"; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
            }

            DesktopShakeAnimation
            {
                id: shake

                target: cell
            }

            contentItem: DesktopLabel
            {
                id: cellText

                text: cell.isVoice ? "" : (root.styledText ? FormsMarkup.colourEndings(cell.modelData, AppTheme.colourAccent) : cell.modelData)
                color: cell.isFilled ? AppTheme.colourPanel : (AppTheme.verbGroupColours[root.verbGroups[cell.index]] ?? AppTheme.colourText)
                textFormat: root.styledText ? Text.StyledText : Text.PlainText
                font.pixelSize: AppMetrics.sessionOptionSize
                font.weight: Font.Medium
                fontSizeMode: Text.Fit
                minimumPixelSize: AppMetrics.fontSmall
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
            }

            background: Rectangle
            {
                radius: AppMetrics.radiusMedium

                color:
                {
                    if (cell.isAnswered || cell.isSelected)
                    {
                        return Qt.tint(AppTheme.colourWell, AppTheme.faded(cell.stateColour, AppTheme.alphaSoft))
                    }

                    if (cell.isFilled)
                    {
                        return AppTheme.colourText
                    }

                    return cell.down && root.interactive ? AppTheme.colourPage : AppTheme.colourWell
                }

                border.color: cell.isAnswered || cell.isSelected ? cell.stateColour
                                                                 : (cell.hovered && root.interactive ? AppTheme.colourOutline : AppTheme.colourBorder)
                border.width: cell.isAnswered || cell.isSelected ? AppMetrics.focusRingWidth : AppMetrics.borderWidth

                Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

                DesktopKeyHint
                {
                    id: hintBox

                    visible: cell.keyHint.length > 0
                    anchors.left: parent.left
                    anchors.leftMargin: AppMetrics.gapMedium
                    anchors.verticalCenter: parent.verticalCenter
                    key: cell.keyHint
                    tint: cell.isAnswered || cell.isSelected ? cell.stateColour
                        : cell.isFilled ? AppTheme.colourPanel
                        : (cell.hovered && root.interactive ? AppTheme.colourSubtle : AppTheme.colourMuted)
                }

                DesktopWaveform
                {
                    visible: cell.isVoice
                    anchors.fill: parent
                    anchors.leftMargin: cell.leftPadding
                    anchors.rightMargin: cell.rightPadding
                    playing: root.playingIndex === cell.index
                    color: playing ? AppTheme.colourAccent : AppTheme.colourSubtle
                }

                DesktopIcon
                {
                    visible: cell.isAnswered
                    anchors.right: parent.right
                    anchors.rightMargin: AppMetrics.gapMedium
                    anchors.verticalCenter: parent.verticalCenter
                    name: cell.isCorrect ? "check" : "close"
                    color: cell.stateColour
                }
            }

            HoverHandler
            {
                enabled: root.interactive
                cursorShape: Qt.PointingHandCursor
            }
        }
    }
}
