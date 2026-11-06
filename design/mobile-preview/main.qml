import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    width: phoneWidth + 220
    height: phoneHeight + 40
    title: "Ikonika — Mobile Preview"

    readonly property int phoneWidth: 412
    readonly property int phoneHeight: 887

    property bool exerciseStripExpanded: false

    readonly property var exerciseScreens: [
        { name: "08a · Flashcard", source: "screens/FlashcardScreen.qml" },
        { name: "08b · Flip", source: "screens/FlipScreen.qml" },
        { name: "08c · Audio transcription", source: "screens/AudioTranscriptionScreen.qml" },
        { name: "08d · Multiple choice", source: "screens/MultipleChoiceScreen.qml" },
        { name: "08e · Speed matching", source: "screens/SpeedMatchingScreen.qml" },
        { name: "08f · Typed", source: "screens/TypedScreen.qml" },
        { name: "08g · Correct/incorrect", source: "screens/CorrectIncorrectScreen.qml" },
        { name: "09a · Triage", source: "screens/TriageScreen.qml" },
        { name: "09b · Review", source: "screens/ReviewScreen.qml" },
        { name: "09c · Exercise complete", source: "screens/ExerciseCompleteScreen.qml" }
    ]

    property var screens: [
        { name: "08a · Flashcard", source: "screens/FlashcardScreen.qml" },
        { name: "08b · Flip", source: "screens/FlipScreen.qml" },
        { name: "08c · Audio transcription", source: "screens/AudioTranscriptionScreen.qml" },
        { name: "08d · Multiple choice", source: "screens/MultipleChoiceScreen.qml" },
        { name: "08e · Speed matching", source: "screens/SpeedMatchingScreen.qml" },
        { name: "08f · Typed", source: "screens/TypedScreen.qml" },
        { name: "08g · Correct/incorrect", source: "screens/CorrectIncorrectScreen.qml" },
		{ name: "09a · Triage", source: "screens/TriageScreen.qml" },
		{ name: "09b · Review", source: "screens/ReviewScreen.qml" },
        { name: "09c · Exercise complete", source: "screens/ExerciseCompleteScreen.qml" },
        { name: "01 · Today", source: "screens/TodayScreen.qml" },
        { name: "02a · Decks", source: "screens/DecksScreen.qml" },
        { name: "10 · Settings", source: "screens/SettingsScreen.qml" },
        { name: "02b · Create deck", source: "screens/CreateDeckScreen.qml" },
        { name: "03a · Deck detail", source: "screens/DeckDetailScreen.qml" },
        { name: "03b · Deck exercises", source: "screens/DeckExercisesScreen.qml" },
        { name: "05a Words · Dictionary", source: "screens/DictionaryScreen.qml" },
        { name: "05d Words · Dict. word", source: "screens/DictionaryWordScreen.qml" },
        { name: "05e Words · Add word", source: "screens/AddWordScreen.qml" },
        { name: "10 · Delete data sheet", source: "screens/DeleteDataSheet.qml" },
        { name: "10 · Language sheet", source: "screens/LanguageSheet.qml" },
        { name: "11 · Word card", source: "screens/WordCardPreviewScreen.qml" },
        { name: "04a Setup · Language", source: "screens/OnboardingLanguageScreen.qml" },
        { name: "04b Setup · Sign in", source: "screens/OnboardingSignInScreen.qml" },
        { name: "04c Setup · Download", source: "screens/OnboardingDownloadScreen.qml" }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Button {
            text: window.exerciseStripExpanded ? "▾ Hide all exercise screens" : "▸ Show all exercise screens"
            onClicked: window.exerciseStripExpanded = !window.exerciseStripExpanded
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: window.phoneHeight + 20
            visible: window.exerciseStripExpanded
            clip: true

            Row {
                spacing: 20

                Repeater {
                    model: window.exerciseScreens
                    delegate: Rectangle {
                        width: window.phoneWidth
                        height: window.phoneHeight
                        border.color: Theme.line
                        border.width: 1
                        clip: true

                        Loader {
                            anchors.fill: parent
                            source: modelData.source
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            ColumnLayout {
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                spacing: 4

                Repeater {
                    model: window.screens
                    delegate: Button {
                        Layout.fillWidth: true
                        text: modelData.name
                        onClicked: loader.source = modelData.source
                    }
                }

                Item { Layout.fillHeight: true }
            }

            Rectangle {
                Layout.preferredWidth: window.phoneWidth
                Layout.preferredHeight: window.phoneHeight
                Layout.alignment: Qt.AlignTop
                border.color: Theme.line
                border.width: 1
                clip: true

                Loader {
                    id: loader
                    anchors.fill: parent
                    source: window.screens[0].source
                }
            }
        }
    }
}
