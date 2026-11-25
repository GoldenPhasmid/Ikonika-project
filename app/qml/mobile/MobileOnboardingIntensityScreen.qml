import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property var store

    MobileOnboardingPage
    {
        anchors.fill: parent

        title: qsTr("Learning Intensity")

        onContinued: AppOnboarding.next()

        Text
        {
            Layout.fillWidth: true
            text: qsTr("Higher intensity: more reviews and shorter breaks in between.")
            font.pixelSize: AppMetrics.fontBody
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourSubtle
            wrapMode: Text.WordWrap
        }

        MobileSegmentedPill
        {
            objectName: "onboarding_intensity"

            Layout.fillWidth: true
            options: [qsTr("Light"), qsTr("Normal"), qsTr("Hard")]
            currentIndex: root.store.learningIntensity

            onChosen: function(index) { root.store.learningIntensity = index }
        }

        ScheduleTimeline
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapMedium
            titleSize: AppMetrics.fontLabel
            showsForgotten: false
            paths: root.store.schedulePreview.slice(0, 1)
        }
    }
}
