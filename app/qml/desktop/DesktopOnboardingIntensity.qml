import QtQuick
import QtQuick.Layouts

DesktopFormPage
{
    id: root

    required property var store

    maximumWidth: AppMetrics.dialogWidth

    DesktopLabel
    {
        Layout.fillWidth: true
        text: qsTr("Learning Intensity")
        font.pixelSize: AppMetrics.fontHeadline
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap

        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        text: qsTr("Higher intensity: more reviews and shorter breaks in between.")
        color: AppTheme.colourMuted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    DesktopSegmentedPill
    {
        objectName: "onboarding_intensity"

        Layout.alignment: Qt.AlignHCenter
        accessibleName: qsTr("Learning Intensity")
        options: [qsTr("Light"), qsTr("Normal"), qsTr("Hard")]
        currentIndex: root.store.learningIntensity

        onChosen: function(index) { root.store.learningIntensity = index }
    }

    ScheduleTimeline
    {
        Layout.fillWidth: true
        Layout.topMargin: AppMetrics.gapMedium
        showsForgotten: false
        paths: root.store.schedulePreview.slice(0, 1)
    }

    footer: [
        DesktopFormButton
        {
            objectName: "onboarding_back"

            text: qsTr("Back")
            keyHint: qsTr("Backspace")
            shortcutKeys: ["Backspace"]

            onClicked: AppOnboarding.back()
        },
        DesktopFormButton
        {
            objectName: "onboarding_continue"

            tone: "primary"
            text: qsTr("Continue")
            keyHint: qsTr("Enter")
            shortcutKeys: ["Return", "Enter"]

            onClicked: AppOnboarding.next()
        }
    ]
}
