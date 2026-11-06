import QtQuick
import QtQuick.Layouts
import "../"

ExerciseShell {
    title: "Match a pair"
    progress: 0
    hasActions: false

    readonly property var cells: [
        "pang", "sudden sharp pain",
        "gimcrack", "(adj) showy thing or a poor quality",
        "fleece", "swindle or cheat out money",
        "contrition", "sincere remorse or regret for past actions",
        "vermin", "accept something as plausible without extensive supporting evidence"
    ]

    ContentArea {
        posVisible: false
        counterText: "4 / 25"
        showPrimary: false
        reserveActionsSpace: true

        secondaryContent: UniformCardGrid {
            Layout.fillWidth: true
            cardModel: cells
        }
    }
}
