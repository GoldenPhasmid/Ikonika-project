import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    default property alias content: column.data
    property string title: ""
    property string continueLabel: qsTr("Continue")
    property bool continueEnabled: true
    readonly property real choiceWidth: (column.width - AppMetrics.gapSmall) / 2

    signal continued()

    color: AppTheme.colourPanel

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        MobileHeader
        {
            showBack: AppOnboarding.canGoBack

            onBackRequested: AppOnboarding.back()
        }

        Flickable
        {
            id: flickable

            Layout.fillWidth: true
            Layout.fillHeight: true

            contentWidth: width
            contentHeight: Math.max(height, column.implicitHeight + AppMetrics.gapLarge * 2)
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout
            {
                id: column

                x: AppMetrics.gapLarge
                y: Math.max(AppMetrics.gapLarge, (flickable.height - column.implicitHeight) / 2)
                width: flickable.width - AppMetrics.gapLarge * 2
                spacing: AppMetrics.gapLarge

                Text
                {
                    Layout.fillWidth: true

                    visible: root.title.length > 0
                    text: root.title
                    font.pixelSize: AppMetrics.fontHeadline
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourText
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap

                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }
            }
        }

        MobileWideButton
        {
            objectName: "onboarding_continue"

            Layout.fillWidth: true
            Layout.margins: AppMetrics.gapLarge

            label: root.continueLabel
            interactive: root.continueEnabled

            onClicked: root.continued()
        }
    }
}
