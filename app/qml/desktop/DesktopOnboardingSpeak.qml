import QtQuick
import QtQuick.Layouts

DesktopFormPage
{
    id: root

    maximumWidth: AppMetrics.dialogWidth

    DesktopLabel
    {
        Layout.fillWidth: true
        Layout.bottomMargin: AppMetrics.gapMedium
        text: qsTr("I speak")
        font.pixelSize: AppMetrics.fontHeadline
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap

        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    DesktopLanguageChoiceGrid
    {
        availableWidth: root.columnWidth

        Repeater
        {
            model: AppLocalization.languageCodes

            delegate: DesktopLanguageChoice
            {
                required property string modelData
                required property int index

                objectName: "onboarding_speak_" + modelData

                code: modelData
                text: AppLocalization.languageNames[index]
                selected: AppOnboarding.spokenLanguage === modelData

                onClicked: AppOnboarding.selectSpokenLanguage(modelData)
            }
        }
    }

    footer: [
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
