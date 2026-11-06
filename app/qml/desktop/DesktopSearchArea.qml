import QtQuick
import QtQuick.Controls.Basic

FocusScope
{
    id: root

    property alias text: area.text
    property alias placeholderText: area.placeholderText
    property int maximumLines: 4
    property bool large: false
    property bool navigatesList: false

    readonly property string term: (area.text + area.preeditText).trim()
    readonly property real rowHeight: root.large ? AppMetrics.buttonHeightLarge : AppMetrics.buttonHeight
    readonly property real lineHeight: Math.ceil(metrics.lineSpacing)
    readonly property real verticalPadding: Math.max(0, (root.rowHeight - root.lineHeight) / 2)

    signal previousRequested()
    signal nextRequested()
    signal submitted()

    function clear()
    {
        area.clear()
    }

    function selectAll()
    {
        area.selectAll()
    }

    implicitHeight: Math.max(root.rowHeight,
                             Math.min(area.implicitHeight, root.lineHeight * root.maximumLines + root.verticalPadding * 2))

    FontMetrics
    {
        id: metrics

        font: area.font
    }

    HoverHandler
    {
        id: hover

        cursorShape: Qt.IBeamCursor
    }

    Rectangle
    {
        anchors.fill: parent

        radius: AppMetrics.radiusMedium
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: area.activeFocus ? AppTheme.colourAccent
                                       : (hover.hovered ? AppTheme.colourOutline : AppTheme.colourBorder)

        Behavior on border.color { ColorAnimation { duration: AppMetrics.motionFast } }
    }

    DesktopIcon
    {
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.gapCompact
        y: (root.rowHeight - height) / 2

        name: "search"
        color: AppTheme.colourMuted
    }

    Flickable
    {
        id: flick

        anchors.fill: parent
        anchors.leftMargin: AppMetrics.gapCompact + AppMetrics.iconSize + AppMetrics.gapSmall
        anchors.rightMargin: clearButton.visible ? clearButton.width + AppMetrics.gapTight : AppMetrics.gapCompact

        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: flick.contentHeight > flick.height

        ScrollBar.vertical: DesktopScrollBar {}

        TextArea.flickable: TextArea
        {
            id: area

            focus: true
            wrapMode: TextEdit.Wrap
            leftPadding: 0
            rightPadding: 0
            topPadding: root.verticalPadding
            bottomPadding: root.verticalPadding

            font.family: AppMetrics.fontFamily
            font.pixelSize: root.large ? AppMetrics.fontLabel : AppMetrics.fontBody
            color: AppTheme.colourText
            placeholderTextColor: AppTheme.colourMuted
            selectionColor: AppTheme.colourAccent
            selectedTextColor: AppTheme.colourAccentText
            selectByMouse: true
            background: null

            Accessible.name: area.placeholderText

            onTextChanged:
            {
                if (area.text.indexOf("\n") >= 0)
                {
                    area.text = area.text.replace(/\s*\n\s*/g, " ")
                    area.cursorPosition = area.length
                }
            }

            Keys.onReturnPressed: function(event)
            {
                event.accepted = true
                root.submitted()
            }

            Keys.onEnterPressed: function(event)
            {
                event.accepted = true
                root.submitted()
            }

            Keys.onUpPressed: function(event)
            {
                event.accepted = root.navigatesList

                if (root.navigatesList)
                {
                    root.previousRequested()
                }
            }

            Keys.onDownPressed: function(event)
            {
                event.accepted = root.navigatesList

                if (root.navigatesList)
                {
                    root.nextRequested()
                }
            }

            Keys.onEscapePressed: function(event)
            {
                if (area.text.length === 0)
                {
                    event.accepted = false

                    return
                }

                area.clear()
            }
        }
    }

    DesktopIconButton
    {
        id: clearButton

        anchors.right: parent.right
        anchors.rightMargin: AppMetrics.gapTight
        y: AppMetrics.gapSmall / 2

        width: root.rowHeight - AppMetrics.gapSmall
        height: width

        visible: area.text.length > 0
        focusPolicy: Qt.NoFocus
        iconName: "close"
        iconColour: AppTheme.colourMuted
        text: qsTr("Clear search")

        onClicked:
        {
            area.clear()
            root.forceActiveFocus()
        }
    }
}
