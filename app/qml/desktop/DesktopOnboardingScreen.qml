import QtQuick
import QtQuick.Layouts

FocusScope
{
    id: root

    required property var store
    required property var placement

    signal finished()

    readonly property var steps: AppSyncController.restoring ? ["restore"] : AppOnboarding.steps
    readonly property string step: AppSyncController.restoring ? "restore" : AppOnboarding.step

    Rectangle
    {
        anchors.fill: parent
        color: AppTheme.colourSunken
    }

    DesktopOnboardingSpeak
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        visible: root.step === "speak"
    }

    DesktopOnboardingSignIn
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        visible: root.step === "signin"

        onContinued: AppOnboarding.next()
    }

    DesktopOnboardingLearn
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        visible: root.step === "learn"
    }

    DesktopOnboardingDaily
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        visible: root.step === "daily"

        store: root.store
    }

    Loader
    {
        id: placementStep

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        active: root.step === "placement"
        visible: active
        focus: active

        sourceComponent: Component
        {
            DesktopOnboardingPlacement
            {
                focus: true

                controller: root.placement
                targetLanguage: AppOnboarding.chosenLanguage
                nativeLanguage: AppOnboarding.chosenNative
                showsBack: AppOnboarding.canGoBack

                onFinished: AppOnboarding.next()
                onBackRequested: AppOnboarding.back()
            }
        }
    }

    DesktopOnboardingRestore
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        visible: root.step === "restore"
    }

    DesktopOnboardingDownload
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        visible: root.step === "download"

        store: root.store

        onDone: root.finished()
    }

    Rectangle
    {
        id: progressBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: AppMetrics.topBarHeight
        color: AppTheme.colourPanel

        DesktopSessionProgress
        {
            anchors.centerIn: parent
            stage: root.steps.indexOf(root.step) + 1
            stageCount: root.steps.length
            stageProgress: root.step === "download"
                ? (AppOnboarding.dictionaryReady ? 1 : (AppPackageDownloader.totalBytes > 0
                    ? AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes : 0))
                : (placementStep.item ? placementStep.item.testProgress : 0)
        }
    }
}
