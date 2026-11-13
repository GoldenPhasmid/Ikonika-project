import QtQuick
import QtQuick.Layouts

DesktopFormPage
{
    id: root

    property string selectedCode: AppOnboarding.chosenLanguage

    readonly property var languages: AppOnboarding.learnableLanguages

    readonly property bool hasSelection: root.languages.some(Language => Language.code === root.selectedCode && Language.compatible)

    function commit()
    {
        AppOnboarding.chooseLearning(root.selectedCode)
        AppOnboarding.next()
    }

    maximumWidth: AppMetrics.dialogWidth

    DesktopLabel
    {
        Layout.fillWidth: true
        Layout.bottomMargin: AppMetrics.gapMedium
        text: qsTr("I want to learn")
        font.pixelSize: AppMetrics.fontHeadline
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap

        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    DesktopLanguageChoiceGrid
    {
        visible: root.languages.length > 0
        availableWidth: root.columnWidth

        Repeater
        {
            model: root.languages

            delegate: DesktopLanguageChoice
            {
                required property var modelData

                objectName: "onboarding_learn_" + modelData.code

                code: modelData.code
                text: modelData.name
                detail: modelData.detail
                enabled: modelData.compatible
                selected: root.selectedCode === modelData.code

                onClicked: root.selectedCode = modelData.code
            }
        }
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        visible: root.languages.length === 0
        text: AppCatalog.busy ? qsTr("Looking for languages")
            : (AppCatalog.lastError.length > 0 ? qsTr("Cannot reach the language list")
                                               : qsTr("Nothing published yet"))
        color: AppTheme.colourMuted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
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
            visible: root.languages.length === 0 && !AppCatalog.busy
            text: qsTr("Try again")

            onClicked: AppOnboarding.retry()
        },
        DesktopFormButton
        {
            objectName: "onboarding_continue"

            tone: "primary"
            text: qsTr("Continue")
            keyHint: qsTr("Enter")
            shortcutKeys: ["Return", "Enter"]
            enabled: root.hasSelection

            onClicked: root.commit()
        }
    ]
}
