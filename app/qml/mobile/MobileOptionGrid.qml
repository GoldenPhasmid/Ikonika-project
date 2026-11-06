import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

GridLayout
{
    id: root

    required property var cardModel
    property var verbGroups: []

    property var borderColours: []
    property var cellOpacities: []
    property bool interactive: true
    property real fitHeight: 0
    property bool styledText: false
    property bool voiceColumn: false
    property int playingIndex: -1

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

    readonly property real cellHeight:
    {
        let tallest = AppMetrics.optionCellMinimumHeight
        let tallestText = 0

        for (let i = 0; i < children.length; i++)
        {
            const cell = children[i]

            if (cell.textItem)
            {
                tallest = Math.max(tallest, cell.textItem.implicitHeight + AppMetrics.gapLarge)
                tallestText = Math.max(tallestText, cell.textItem.implicitHeight + AppMetrics.gapLarge)
            }
        }

        const rows = Math.ceil(root.cardModel.length / root.columns)

        if (root.fitHeight <= 0 || rows <= 0)
        {
            return tallest
        }

        const fitted = Math.floor((root.fitHeight - root.rowSpacing * (rows - 1)) / rows)

        return Math.min(tallest, Math.max(fitted, tallestText))
    }

    Repeater
    {
        model: root.cardModel

        delegate: Rectangle
        {
            id: card

            required property string modelData
            required property int index

            property alias textItem: cardText

            readonly property color stateColour: card.index < root.borderColours.length
                ? root.borderColours[card.index]
                : AppTheme.colourBorder

            readonly property bool isCorrect: card.stateColour === AppTheme.colourCorrect
            readonly property bool isWrong: card.stateColour === AppTheme.colourWrong
            readonly property bool isAnswered: card.isCorrect || card.isWrong
            readonly property bool isVoice: root.voiceColumn && card.index % root.columns === 0 && !card.isCorrect

            Layout.fillWidth: true
            Layout.preferredHeight: root.cellHeight

            radius: AppMetrics.radiusMedium
            color: card.isAnswered
                ? Qt.tint(AppTheme.colourPanel, Qt.rgba(card.stateColour.r, card.stateColour.g,
                                                        card.stateColour.b, 0.12))
                : AppTheme.colourPanel
            border.color: card.stateColour
            border.width: card.isAnswered ? AppMetrics.borderWidth * 2 : AppMetrics.borderWidth

            opacity: card.index < root.cellOpacities.length
                ? root.cellOpacities[card.index]
                : 1.0

            transform: MobileShake { id: shake }

            onIsCorrectChanged: if (card.isCorrect && root.popArmed) pop.restart()
            onIsWrongChanged: if (card.isWrong && root.shakeArmed) shake.play()

            SequentialAnimation
            {
                id: pop

                NumberAnimation { target: card; property: "scale"; to: 0.94; duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
                NumberAnimation { target: card; property: "scale"; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
            }

            Behavior on border.color
            {
                enabled: card.isAnswered

                ColorAnimation { duration: AppMetrics.motionBase }
            }
            Behavior on opacity { NumberAnimation { duration: AppMetrics.motionBase } }

            Text
            {
                id: cardText

                anchors.centerIn: parent
                width: parent.width - (AppMetrics.gapCompact * 2)

                visible: !card.isVoice
                text: card.isVoice ? "" : card.modelData
                textFormat: root.styledText ? Text.StyledText : Text.PlainText
                linkColor: AppTheme.colourAccent
                font.pixelSize: AppMetrics.sessionOptionSize
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.verbGroupColours[root.verbGroups[card.index]] ?? AppTheme.colourText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            MobileWaveform
            {
                anchors.fill: parent
                visible: card.isVoice
                playing: root.playingIndex === card.index
            }

            MobileVectorIcon
            {
                visible: card.isAnswered

                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: AppMetrics.gapSmall

                width: AppMetrics.fontLabel
                height: AppMetrics.fontLabel

                svgPath: card.isCorrect ? Icons.Icons.check.path : Icons.Icons.close.path
                color: card.stateColour
            }

            MouseArea
            {
                anchors.fill: parent
                enabled: root.interactive
                onClicked: root.optionClicked(card.index)
            }
        }
    }
}
