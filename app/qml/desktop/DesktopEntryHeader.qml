import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property string headword: ""
    property string meta: ""
    property string phonetic: ""
    property string frequencyBucket: ""
    property string cefrLevel: ""
    property bool centred: false
    property bool webSearchShown: true
    property int headwordSize: AppMetrics.dictionaryFontHeadline
    property int gender: -1
    property string verbGroup: ""
    property string verbGroupLabel: ""
    property var decks: []

    property alias actions: actionRow.data
    property alias status: statusRow.data

    signal speakRequested()
    signal webSearchRequested()
    signal deckOpenRequested(int deckId)

    function speak()
    {
        speakButton.speak()
    }

    readonly property bool stacked: root.width > 0 && root.width < AppMetrics.twoColumnMinimumWidth

    readonly property real badgesWidth:
    {
        let total = 0
        let count = 0

        for (const child of badges.children)
        {
            if (child.visible && child.implicitWidth > 0)
            {
                total += child.implicitWidth
                ++count
            }
        }

        return total + Math.max(0, count - 1) * badges.spacing
    }

    readonly property string shownPhonetic: root.phonetic.length === 0 || root.phonetic.startsWith("/")
                                            || root.phonetic.startsWith("[")
        ? root.phonetic : qsTr("/%1/").arg(root.phonetic)

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    GridLayout
    {
        Layout.fillWidth: true
        columns: root.stacked ? 2 : 4
        columnSpacing: AppMetrics.gapSmall
        rowSpacing: AppMetrics.gapSmall

        Item
        {
            visible: root.centred && !root.stacked
            Layout.preferredWidth: tools.implicitWidth + AppMetrics.gapSmall + actionRow.implicitWidth
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.headword
            color: AppTheme.verbGroupColours[root.verbGroup] ?? AppTheme.genderColours[root.gender] ?? AppTheme.colourText
            font.pixelSize: root.headwordSize
            font.weight: Font.DemiBold
            horizontalAlignment: root.centred ? Text.AlignHCenter : Text.AlignLeft
            wrapMode: root.stacked ? Text.Wrap : Text.NoWrap
            elide: root.stacked ? Text.ElideNone : Text.ElideRight

            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        RowLayout
        {
            id: tools

            Layout.fillWidth: false
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            spacing: AppMetrics.gapSmall

            DesktopSpeakButton
            {
                id: speakButton

                text: qsTr("Pronounce (Ctrl+Space)")

                onSpeakRequested: root.speakRequested()
            }

            DesktopIconButton
            {
                visible: root.webSearchShown
                iconName: "external_link"
                text: qsTr("Search the web")

                onClicked: root.webSearchRequested()
            }
        }

        RowLayout
        {
            id: actionRow

            Layout.columnSpan: root.stacked ? 2 : 1
            Layout.fillWidth: root.stacked
            spacing: AppMetrics.gapSmall
        }
    }

    GridLayout
    {
        Layout.fillWidth: true
        columns: root.stacked ? 1 : 3
        columnSpacing: AppMetrics.gapSmall
        rowSpacing: AppMetrics.gapSmall

        Item
        {
            visible: root.centred && !root.stacked
            Layout.preferredWidth: root.badgesWidth
        }

        Item
        {
            Layout.fillWidth: true
            visible: !root.stacked || details.implicitWidth > 0
            implicitHeight: details.implicitHeight

            Row
            {
                id: details

                x: root.centred ? (parent.width - width) / 2 : 0
                spacing: AppMetrics.gapSmall

                DesktopLabel
                {
                    visible: text.length > 0
                    text: root.meta
                    font.italic: true
                    color: AppTheme.colourSubtle
                }

                DesktopLabel
                {
                    visible: text.length > 0
                    text: root.verbGroupLabel
                    color: AppTheme.verbGroupColours[root.verbGroup] ?? AppTheme.colourSubtle
                    font.pixelSize: AppMetrics.fontSmall
                    font.weight: Font.Medium
                }

                DesktopLabel
                {
                    visible: text.length > 0
                    text: root.shownPhonetic
                    color: AppTheme.colourMuted
                }
            }
        }

        Flow
        {
            id: badges

            Layout.fillWidth: root.stacked
            Layout.preferredWidth: root.stacked ? -1 : root.badgesWidth
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            spacing: AppMetrics.gapSmall

            Item
            {
                implicitWidth: statusRow.visibleChildren.length > 0 ? statusRow.implicitWidth : 0
                implicitHeight: Math.max(AppMetrics.badgeHeight, statusRow.implicitHeight)

                RowLayout
                {
                    id: statusRow

                    anchors.verticalCenter: parent.verticalCenter
                    spacing: AppMetrics.gapSmall
                }
            }

            Repeater
            {
                model: root.decks

                delegate: DesktopDeckChip
                {
                    required property var modelData

                    objectName: "entry_deck_" + modelData.id

                    text: modelData.name
                    iconName: modelData.icon
                    tint: modelData.tint

                    onClicked: root.deckOpenRequested(modelData.id)
                }
            }

            DesktopLevelBadge
            {
                label: root.frequencyBucket
                fill: AppTheme.frequencyColours[label] || AppTheme.colourBorder
            }

            DesktopLevelBadge
            {
                label: root.cefrLevel
                fill: AppTheme.cefrColours[label] || AppTheme.colourBorder
            }
        }
    }
}
