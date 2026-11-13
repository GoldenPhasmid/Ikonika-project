import QtQuick
import QtQuick.Layouts

MobileOnboardingPage
{
    id: root

    title: qsTr("I speak")

    onContinued: AppOnboarding.next()

    GridLayout
    {
        Layout.alignment: Qt.AlignHCenter
        columns: 2
        columnSpacing: AppMetrics.gapSmall
        rowSpacing: AppMetrics.gapSmall

        Repeater
        {
            model: AppLocalization.languageCodes

            delegate: MobileLanguageChoice
            {
                required property string modelData
                required property int index

                objectName: "onboarding_speak_" + modelData

                tileWidth: root.choiceWidth
                code: modelData
                label: AppLocalization.languageNames[index]
                selected: AppOnboarding.spokenLanguage === modelData

                onClicked: AppOnboarding.selectSpokenLanguage(modelData)
            }
        }
    }
}
