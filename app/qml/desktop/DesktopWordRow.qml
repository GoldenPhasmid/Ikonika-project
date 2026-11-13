import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ItemDelegate
{
    id: root

    property string word: ""
    property string translation: ""
    property string partOfSpeech: ""
    property int gender: -1
    property string verbGroup: ""
    property string cefrLevel: ""
    property int stage: -1
    property bool completed: false
    property int maxStage: 1
    property bool selected: false
    property bool italicPartOfSpeech: true

    property bool canEdit: false
    property bool canComplete: false
    property bool canReset: false
    property bool canRemove: false

    signal editRequested()
    signal completeRequested()
    signal resetRequested()
    signal removeRequested()

    readonly property bool learning: root.stage >= 0 || root.completed
    readonly property bool hasMenu: root.canEdit || root.canComplete || root.canReset || root.canRemove
    readonly property bool showTools: root.hovered || menu.visible

    width: ListView.view ? ListView.view.rowWidth : implicitWidth
    implicitHeight: AppMetrics.listRowHeight

    leftPadding: AppMetrics.gapMedium
    rightPadding: AppMetrics.gapSmall
    topPadding: 0
    bottomPadding: 0

    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    highlighted: root.selected

    text: root.word
    Accessible.role: Accessible.ListItem
    Accessible.name: root.translation.length > 0 ? qsTr("%1, %2").arg(root.word).arg(root.translation) : root.word
    Accessible.selected: root.selected

    function openMenu(x, y)
    {
        if (root.hasMenu)
        {
            menu.popup(root, x, y)
        }
    }

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapCompact

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: 0

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.word
                color: AppTheme.verbGroupColours[root.verbGroup] ?? AppTheme.genderColours[root.gender] ?? AppTheme.colourText
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: root.translation.length > 0
                text: root.translation
                font.pixelSize: AppMetrics.fontBody
                color: AppTheme.colourText
                elide: Text.ElideRight
            }
        }

        DesktopIconButton
        {
            visible: root.showTools && root.hasMenu
            focusPolicy: Qt.NoFocus
            iconName: "kebab"
            iconColour: AppTheme.colourMuted
            text: qsTr("Word options")

            onClicked: root.openMenu(x, y + height)
        }

        DesktopLabel
        {
            visible: !(root.showTools && root.hasMenu) && root.partOfSpeech.length > 0
            text: root.partOfSpeech
            font.pixelSize: AppMetrics.fontSmall
            font.italic: root.italicPartOfSpeech
            color: AppTheme.colourSubtle
        }

        Item
        {
            visible: root.learning
            implicitWidth: AppMetrics.progressRingSize
            implicitHeight: AppMetrics.progressRingSize

            DesktopProgressRing
            {
                anchors.centerIn: parent
                visible: root.learning
                stage: root.stage
                stageCount: root.maxStage
                completed: root.completed
            }
        }

        DesktopLevelBadge
        {
            visible: root.cefrLevel.length > 0
            label: root.cefrLevel
            fill: AppTheme.cefrColours[label] || AppTheme.colourBorder
        }
    }

    background: DesktopSelectionBackground
    {
        selected: root.selected
        hovered: root.hovered
        pressed: root.down
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler
    {
        acceptedButtons: Qt.RightButton
        onTapped: function(eventPoint) { root.openMenu(eventPoint.position.x, eventPoint.position.y) }
    }

    DesktopMenu
    {
        id: menu

        DesktopMenuItem
        {
            text: qsTr("Edit")
            iconName: "edit"
            enabled: root.canEdit
            height: root.canEdit ? implicitHeight : 0
            visible: root.canEdit
            onTriggered: root.editRequested()
        }

        DesktopMenuItem
        {
            text: qsTr("Mark as learned")
            iconName: "check"
            enabled: root.canComplete
            height: root.canComplete ? implicitHeight : 0
            visible: root.canComplete
            onTriggered: root.completeRequested()
        }

        DesktopMenuItem
        {
            text: qsTr("Reset progress")
            iconName: "undo"
            enabled: root.canReset
            height: root.canReset ? implicitHeight : 0
            visible: root.canReset
            onTriggered: root.resetRequested()
        }

        DesktopMenuItem
        {
            text: qsTr("Remove")
            iconName: "trash"
            destructive: true
            enabled: root.canRemove
            height: root.canRemove ? implicitHeight : 0
            visible: root.canRemove
            onTriggered: root.removeRequested()
        }
    }
}
