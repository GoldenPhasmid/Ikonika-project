import QtQuick
import QtQuick.Layouts

FocusScope
{
    id: root

    required property var controller

    property string targetLanguage: ""
    property string nativeLanguage: ""
    property string phase: "intro"
    property bool standalone: false
    property bool retake: false

    signal finished()

    readonly property real testProgress: root.phase === "test" ? root.controller.exerciseProgress
                                                              : (root.phase === "result" ? 1 : 0)

    readonly property string languageName:
    {
        const found = AppCatalog.languages.find(Language => Language.code === root.targetLanguage)

        return found ? found.name : root.targetLanguage.toUpperCase()
    }

    readonly property bool showsResult: root.phase === "result"
    readonly property color levelColour: AppTheme.cefrColours[root.controller.cefrLevel] || AppTheme.colourBorder

    readonly property var stats: [
        { value: Number(root.controller.knownWordCount).toLocaleString(Qt.locale(), "f", 0), label: qsTr("words you know, roughly") },
        { value: qsTr("%1 / %2").arg(root.controller.knownAnswerCount).arg(root.controller.answeredCount), label: qsTr("words you knew here") }
    ]

    function begin()
    {
        if (root.controller.start(root.targetLanguage, root.nativeLanguage))
        {
            root.phase = "test"
            exercise.forceActiveFocus()

            return
        }

        root.skip()
    }

    function skip()
    {
        if (!root.retake)
        {
            root.controller.skip(root.targetLanguage)
        }

        root.leave()
    }

    function leave()
    {
        root.phase = "intro"
        root.finished()
    }

    Connections
    {
        target: root.controller

        function onStateChanged()
        {
            if (root.phase === "test" && root.controller.finished)
            {
                root.phase = "result"
            }
        }
    }

    Rectangle
    {
        anchors.fill: parent
        color: AppTheme.colourSunken
    }

    DesktopFormPage
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: ownProgress.bottom
        anchors.bottom: parent.bottom
        visible: root.phase !== "test"

        maximumWidth: AppMetrics.dialogWidth

        Rectangle
        {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: AppMetrics.completeBadgeSize
            Layout.preferredHeight: AppMetrics.completeBadgeSize

            visible: root.showsResult
            radius: width / 2
            color: Qt.tint(AppTheme.colourPanel, AppTheme.faded(root.levelColour, AppTheme.alphaMedium))

            DesktopLabel
            {
                anchors.centerIn: parent
                text: root.controller.cefrLevel
                font.pixelSize: AppMetrics.statValueSize
                font.weight: Font.Bold
                color: root.levelColour
            }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.showsResult ? qsTr("Your %1 level is %2").arg(root.languageName).arg(root.controller.cefrLevel)
                                   : qsTr("Let's determine your %1 level").arg(root.languageName)
            font.pixelSize: AppMetrics.fontHeadline
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap

            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        RowLayout
        {
            Layout.fillWidth: true
            visible: root.showsResult
            spacing: AppMetrics.gapMedium

            Repeater
            {
                model: root.stats

                delegate: Rectangle
                {
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: AppMetrics.statBoxHeight

                    radius: AppMetrics.radiusLarge
                    color: AppTheme.colourPanel
                    border.width: AppMetrics.borderWidth
                    border.color: AppTheme.colourBorder

                    Accessible.role: Accessible.StaticText
                    Accessible.name: qsTr("%1 %2").arg(modelData.value).arg(modelData.label)

                    ColumnLayout
                    {
                        anchors.centerIn: parent
                        spacing: AppMetrics.gapTight

                        DesktopLabel
                        {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.value
                            font.pixelSize: AppMetrics.statValueSize
                            font.weight: Font.DemiBold
                        }

                        DesktopLabel
                        {
                            Layout.alignment: Qt.AlignHCenter
                            text: modelData.label
                            font.pixelSize: AppMetrics.fontSmall
                            color: AppTheme.colourMuted
                        }
                    }
                }
            }
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            visible: root.showsResult
            spacing: AppMetrics.gapSmall

            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("%1 of %2 words known, from common to rare").arg(root.controller.knownAnswerCount).arg(root.controller.answeredCount)

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapTight

                Repeater
                {
                    model: root.controller.answers

                    delegate: Rectangle
                    {
                        required property bool modelData

                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: AppMetrics.badgeHeight

                        radius: AppMetrics.radiusSmall
                        color: modelData ? AppTheme.colourCorrect : AppTheme.colourWrong
                    }
                }
            }

            RowLayout
            {
                Layout.fillWidth: true

                DesktopLabel
                {
                    Layout.fillWidth: true
                    text: qsTr("Common words")
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted
                }

                DesktopLabel
                {
                    text: qsTr("Rare words")
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted
                }
            }
        }

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapMedium

            DesktopButton
            {
                objectName: "placement_skip"

                Layout.fillWidth: true
                Layout.preferredWidth: 1
                visible: root.phase === "intro"
                large: true
                text: root.retake ? qsTr("Cancel") : qsTr("Skip the test")

                onClicked: root.skip()
            }

            DesktopButton
            {
                objectName: "placement_start"

                Layout.fillWidth: true
                Layout.preferredWidth: 1
                large: true
                tone: "primary"
                text: root.showsResult ? qsTr("Continue") : qsTr("Start the test")
                trailingIconName: "arrow_right"

                onClicked: root.showsResult ? root.leave() : root.begin()
            }
        }
    }

    DesktopReviewExercise
    {
        id: exercise

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: ownProgress.bottom
        anchors.bottom: parent.bottom
        visible: root.phase === "test"

        controller: root.controller
        editable: false

        incorrectLabel: qsTr("I did not know it")
        correctLabel: qsTr("I knew it")

        Keys.onPressed: function(event)
        {
            if (event.key === Qt.Key_Escape)
            {
                root.phase = "intro"
                event.accepted = true

                return
            }

            event.accepted = exercise.handleKey(event)
        }
    }

    DesktopSessionProgress
    {
        id: ownProgress

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        visible: root.standalone
        height: root.standalone ? AppMetrics.topBarHeight : 0

        stage: 1
        stageCount: 1
        stageProgress: root.testProgress
    }
}
