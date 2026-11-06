import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Rectangle
{
    id: root

    required property string word
    property string translation: ""
    property string cefrLevel: ""
    property string partOfSpeech: ""
    property int gender: -1
    property string verbGroup: ""

    property int stage: -1
    property bool completed: false
    required property int maxStage

    property bool showVoice: true
    property bool showProgress: true
    property bool showTranslation: true
    property bool showCefr: true
    property bool showPartOfSpeech: false

    property bool showAddAction: false
    property bool showLookAction: false
    property bool showOpenAction: false
    property bool showEditAction: true
    property bool showDeleteAction: true
    property bool showCompleteAction: false
    property bool showResetAction: false

    property var coordinator: null

    signal clicked()
    signal voicePlayRequested()
    signal addRequested()
    signal lookRequested()
    signal openRequested()
    signal editRequested()
    signal deleteRequested()
    signal completeRequested()
    signal resetRequested()

    function close()
    {
        front.x = 0

        if (root.coordinator && root.coordinator.openRow === root)
        {
            root.coordinator.openRow = null
        }
    }

    readonly property bool learning: root.stage >= 0 || root.completed

    readonly property int actionWidth: 64

    readonly property bool canComplete: root.showCompleteAction && !root.completed
    readonly property bool canReset: root.showResetAction && (root.stage > 0 || root.completed)

    readonly property real actionsWidth: root.actionWidth *
        ((root.showVoice ? 1 : 0) + (root.showAddAction ? 1 : 0) + (root.showLookAction ? 1 : 0) +
         (root.showOpenAction ? 1 : 0) +
         (root.showEditAction ? 1 : 0) +
         (root.canComplete ? 1 : 0) + (root.canReset ? 1 : 0) + (root.showDeleteAction ? 1 : 0))

    implicitHeight: 56
    height: implicitHeight

    radius: AppMetrics.radiusMedium
    color: AppTheme.colourPanel
    border.color: AppTheme.colourBorder
    border.width: AppMetrics.borderWidth
    clip: true

    Item
    {
        id: revealMask

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: -front.x
        clip: true

        Row
        {
            id: actionsRow

            anchors.right: parent.right
            height: parent.height
            width: root.actionsWidth

            MobileWordCardAction
            {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.showVoice

                iconSvg: Icons.Icons.speaker_on.path
                tint: AppTheme.colourFaint
                iconColor: AppTheme.colourText

                onTriggered: { root.close(); root.voicePlayRequested() }
            }

            MobileWordCardAction
            {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.showAddAction

                icon: "+"
                tint: AppTheme.colourAccent
                iconColor: AppTheme.colourPanel

                onTriggered: { root.close(); root.addRequested() }
            }

            MobileWordCardAction
            {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.showLookAction

                iconSvg: Icons.Icons.search.path
                tint: AppTheme.colourFaint
                iconColor: AppTheme.colourText

                onTriggered: { root.close(); root.lookRequested() }
            }

            MobileWordCardAction
            {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.showOpenAction

                iconSvg: Icons.Icons.external_link.path
                tint: AppTheme.colourFaint
                iconColor: AppTheme.colourText

                onTriggered: { root.close(); root.openRequested() }
            }

            MobileWordCardAction
            {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.showEditAction

                iconSvg: Icons.Icons.edit.path
                tint: AppTheme.colourFaint
                iconColor: AppTheme.colourText

                onTriggered: { root.close(); root.editRequested() }
            }

            MobileWordCardAction
            {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.canComplete

                iconSvg: Icons.Icons.check.path
                tint: AppTheme.colourCorrect
                iconColor: AppTheme.colourPanel

                onTriggered: { root.close(); root.completeRequested() }
            }

            MobileWordCardAction
            {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.canReset
                holdToConfirm: true

                iconSvg: Icons.Icons.undo.path
                tint: AppTheme.colourWarn
                iconColor: AppTheme.colourPanel

                onTriggered: { root.close(); root.resetRequested() }
            }

            MobileWordCardAction
            {
                width: root.actionWidth
                height: actionsRow.height
                visible: root.showDeleteAction
                holdToConfirm: true

                iconSvg: Icons.Icons.trash.path
                tint: AppTheme.colourWrong
                iconColor: AppTheme.colourPanel

                onTriggered: { root.close(); root.deleteRequested() }
            }
        }
    }

    Item
    {
        id: front

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width

        Behavior on x
        {
            enabled: !dragHandler.active
            NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
        }

        DragHandler
        {
            id: dragHandler

            target: front
            yAxis.enabled: false
            xAxis.enabled: true
            xAxis.minimum: -root.actionsWidth
            xAxis.maximum: 0

            onActiveChanged:
            {
                if (dragHandler.active || root.actionsWidth <= 0)
                {
                    return
                }

                if (front.x >= -root.actionsWidth / 2)
                {
                    root.close()
                    return
                }

                front.x = -root.actionsWidth

                if (!root.coordinator)
                {
                    return
                }

                if (root.coordinator.openRow && root.coordinator.openRow !== root)
                {
                    root.coordinator.openRow.close()
                }

                root.coordinator.openRow = root
            }
        }

        RowLayout
        {
            anchors.fill: parent
            anchors.leftMargin: AppMetrics.contentPadding
            anchors.rightMargin: AppMetrics.gapCompact
            spacing: AppMetrics.gapSmall

            ColumnLayout
            {
                Layout.fillWidth: true
                spacing: 2

                Text
                {
                    Layout.fillWidth: true
                    text: root.word
                    font.pixelSize: AppMetrics.fontBody
                    font.weight: Font.DemiBold
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.verbGroupColours[root.verbGroup] ?? AppTheme.genderColours[root.gender] ?? AppTheme.colourText
                    elide: Text.ElideRight
                }

                Text
                {
                    Layout.fillWidth: true
                    visible: root.showTranslation && root.translation.length > 0
                    text: root.translation
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourText
                    elide: Text.ElideRight
                }
            }

            Item { Layout.fillWidth: true }

            Text
            {
                visible: root.showPartOfSpeech && root.partOfSpeech.length > 0
                text: root.partOfSpeech
                font.pixelSize: AppMetrics.fontSmall
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourSubtle
            }

            MobileProgressRing
            {
                visible: root.showProgress && root.learning
                stage: root.stage
                stageCount: root.maxStage
                completed: root.completed
            }

            MobileCefrBadge
            {
                visible: root.showCefr && root.cefrLevel.length > 0
                level: root.cefrLevel
            }
        }

        TapHandler
        {
            acceptedButtons: Qt.LeftButton

            onTapped:
            {
                if (front.x < 0)
                {
                    root.close()
                    return
                }

                root.clicked()
            }
        }
    }
}
