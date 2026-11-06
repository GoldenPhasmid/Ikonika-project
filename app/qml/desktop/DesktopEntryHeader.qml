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
    property int headwordSize: AppMetrics.dictionaryFontHeadline
    property int gender: -1
    property string verbGroup: ""
    property string verbGroupLabel: ""

    property alias actions: actionRow.data
    property alias status: statusRow.data

    signal speakRequested()
    signal webSearchRequested()

    function speak()
    {
        speakButton.speak()
    }

    readonly property string shownPhonetic: root.phonetic.length === 0 || root.phonetic.startsWith("/")
                                            || root.phonetic.startsWith("[")
        ? root.phonetic : qsTr("/%1/").arg(root.phonetic)

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        Item
        {
            visible: root.centred
            Layout.preferredWidth: tools.implicitWidth
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.headword
            color: AppTheme.verbGroupColours[root.verbGroup] ?? AppTheme.genderColours[root.gender] ?? AppTheme.colourText
            font.pixelSize: root.headwordSize
            font.weight: Font.DemiBold
            horizontalAlignment: root.centred ? Text.AlignHCenter : Text.AlignLeft
            elide: Text.ElideRight

            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        RowLayout
        {
            id: tools

            Layout.fillWidth: false
            spacing: AppMetrics.gapSmall

            DesktopSpeakButton
            {
                id: speakButton

                text: qsTr("Pronounce (Ctrl+Space)")

                onSpeakRequested: root.speakRequested()
            }

            DesktopIconButton
            {
                iconName: "external_link"
                text: qsTr("Search the web")

                onClicked: root.webSearchRequested()
            }

            RowLayout
            {
                id: actionRow

                Layout.fillWidth: false
                spacing: AppMetrics.gapSmall
            }
        }
    }

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        Item
        {
            visible: root.centred
            Layout.preferredWidth: badges.implicitWidth
        }

        Item
        {
            Layout.fillWidth: true
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

        RowLayout
        {
            id: badges

            Layout.fillWidth: false
            spacing: AppMetrics.gapSmall

            RowLayout
            {
                id: statusRow

                Layout.fillWidth: false
                spacing: AppMetrics.gapSmall
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
