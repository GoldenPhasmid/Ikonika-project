import QtQuick
import QtQuick.Layouts

MobileOnboardingPage
{
    id: root

    property string selectedCode: AppOnboarding.chosenLanguage

    readonly property var languages: AppOnboarding.learnableLanguages

    readonly property bool hasSelection: root.languages.some(Language => Language.code === root.selectedCode && Language.compatible)

    title: qsTr("I want to learn")
    continueLabel: root.languages.length === 0 && !AppCatalog.busy ? qsTr("Try again") : qsTr("Continue")
    continueEnabled: root.hasSelection || (root.languages.length === 0 && !AppCatalog.busy)

    onContinued:
    {
        if (!root.hasSelection)
        {
            AppOnboarding.retry()

            return
        }

        AppOnboarding.chooseLearning(root.selectedCode)
        AppOnboarding.next()
    }

    GridLayout
    {
        Layout.alignment: Qt.AlignHCenter
        visible: root.languages.length > 0
        columns: 2
        columnSpacing: AppMetrics.gapSmall
        rowSpacing: AppMetrics.gapSmall

        Repeater
        {
            model: root.languages

            delegate: MobileLanguageChoice
            {
                required property var modelData

                objectName: "onboarding_learn_" + modelData.code

                tileWidth: root.choiceWidth
                code: modelData.code
                label: modelData.name
                detail: modelData.detail
                interactive: modelData.compatible
                selected: root.selectedCode === modelData.code

                onClicked: root.selectedCode = modelData.code
            }
        }
    }

    Text
    {
        Layout.fillWidth: true

        visible: root.languages.length === 0
        text: AppCatalog.busy ? qsTr("Looking for languages")
            : (AppCatalog.lastError.length > 0 ? qsTr("Cannot reach the language list")
                                               : qsTr("Nothing published yet"))
        font.pixelSize: AppMetrics.fontSmall
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourMuted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
