import QtQuick
import QtQuick.Layouts

FocusScope
{
    id: root

    required property var store

    property bool signInDone: false
    property bool setupDone: false
    property bool placementDone: false

    required property var placement

    signal finished()

    onVisibleChanged:
    {
        if (!root.visible)
        {
            return
        }

        root.signInDone = false
        root.setupDone = false
        root.placementDone = false
    }

    readonly property bool offersSignIn: AppSyncController.configured
    readonly property var steps: AppSyncController.restoring ? ["restore"]
                                 : AppOnboarding.resumingLanguage ? ["download"]
                                 : (root.offersSignIn ? ["signin", "setup", "placement", "download"]
                                                      : ["setup", "placement", "download"])

    readonly property string step:
    {
        if (AppSyncController.restoring)
        {
            return "restore"
        }

        if (AppOnboarding.resumingLanguage)
        {
            return "download"
        }

        if (root.offersSignIn && !root.signInDone)
        {
            return "signin"
        }

        if (!root.setupDone)
        {
            return "setup"
        }

        return root.placementDone ? "download" : "placement"
    }

    Rectangle
    {
        anchors.fill: parent
        color: AppTheme.colourSunken
    }

    DesktopOnboardingSignIn
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        visible: root.step === "signin"

        onContinued: root.signInDone = true
    }

    DesktopOnboardingSetup
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        visible: root.step === "setup"

        store: root.store

        onContinued:
        {
            root.placementDone = !root.placement.isOffered(AppOnboarding.chosenLanguage, AppOnboarding.chosenNative)
            root.setupDone = true
        }
    }

    DesktopOnboardingPlacement
    {
        id: placementStep

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: progressBar.bottom
        anchors.bottom: parent.bottom
        visible: root.step === "placement"
        focus: visible

        controller: root.placement
        targetLanguage: AppOnboarding.chosenLanguage
        nativeLanguage: AppOnboarding.chosenNative

        onFinished: root.placementDone = true
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
                : (root.step === "placement" ? placementStep.testProgress : 0)
        }
    }
}
