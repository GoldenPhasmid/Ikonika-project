import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property var controller

    property string targetLanguage: ""
    property string nativeLanguage: ""
    property string phase: "intro"
    property bool retake: false
    property bool showsBack: false

    signal finished()
    signal backRequested()

    readonly property string languageName:
    {
        for (const entry of AppCatalog.languages)
        {
            if (entry.code === root.targetLanguage)
            {
                return entry.name
            }
        }

        return root.targetLanguage.toUpperCase()
    }

    readonly property bool showsResult: root.phase === "result"
    readonly property color levelColour: AppTheme.cefrColours[root.controller.cefrLevel] || AppTheme.colourBorder

    readonly property var stats: [
        { value: Number(root.controller.knownWordCount).toLocaleString(Qt.locale(), "f", 0), label: qsTr("words you know, roughly") },
        { value: qsTr("%1 / %2").arg(root.controller.knownAnswerCount).arg(root.controller.answeredCount), label: qsTr("words you knew here") }
    ]

    readonly property var levels: [
        { name: "placement_beginner", label: qsTr("I'm a beginner") },
        { name: "placement_intermediate", label: qsTr("I'm intermediate") },
        { name: "placement_advanced", label: qsTr("I'm advanced") }
    ]

    function begin(level)
    {
        if (root.controller.start(root.targetLanguage, root.nativeLanguage, level))
        {
            root.phase = "test"

            return
        }

        root.chooseNew()
    }

    function chooseNew()
    {
        root.controller.markNew(root.targetLanguage)
        root.leave()
    }

    function leave()
    {
        root.phase = "intro"
        root.finished()
    }

    color: AppTheme.colourPanel

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

    ColumnLayout
    {
        anchors.fill: parent
        visible: root.phase !== "test"
        spacing: 0

        MobileHeader
        {
            showBack: root.showsBack && root.phase === "intro"

            onBackRequested: root.backRequested()
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: AppMetrics.gapLarge
            spacing: AppMetrics.gapLarge

            Item { Layout.fillHeight: true }

            Rectangle
            {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: AppMetrics.statBoxHeight
                Layout.preferredHeight: AppMetrics.statBoxHeight

                visible: root.showsResult
                radius: width / 2
                color: Qt.tint(AppTheme.colourPanel, AppTheme.faded(root.levelColour, AppTheme.alphaMedium))

                Text
                {
                    anchors.centerIn: parent

                    text: root.controller.cefrLevel
                    font.pixelSize: AppMetrics.statValueSize
                    font.weight: Font.Bold
                    font.family: AppMetrics.fontFamily
                    color: root.levelColour
                }
            }

            Text
            {
                Layout.fillWidth: true

                text: root.showsResult ? qsTr("Your %1 level is %2").arg(root.languageName).arg(root.controller.cefrLevel)
                                       : qsTr("How well do you know %1?").arg(root.languageName)
                font.pixelSize: AppMetrics.fontHeadline
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
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

                        radius: AppMetrics.radiusMedium
                        color: AppTheme.colourSunken
                        border.color: AppTheme.colourBorder
                        border.width: AppMetrics.borderWidth

                        ColumnLayout
                        {
                            anchors.centerIn: parent
                            width: parent.width - AppMetrics.gapMedium * 2
                            spacing: AppMetrics.gapTight

                            Text
                            {
                                Layout.fillWidth: true

                                text: modelData.value
                                font.pixelSize: AppMetrics.statValueSize
                                font.weight: Font.DemiBold
                                font.family: AppMetrics.fontFamily
                                color: AppTheme.colourText
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Text
                            {
                                Layout.fillWidth: true

                                text: modelData.label
                                font.pixelSize: AppMetrics.fontMeta
                                font.family: AppMetrics.fontFamily
                                color: AppTheme.colourMuted
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
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
                            Layout.preferredHeight: AppMetrics.statBoxHeight / 2

                            radius: AppMetrics.radiusSmall
                            color: modelData ? AppTheme.colourCorrect : AppTheme.colourWrong
                        }
                    }
                }

                RowLayout
                {
                    Layout.fillWidth: true

                    Text
                    {
                        Layout.fillWidth: true

                        text: qsTr("Common words")
                        font.pixelSize: AppMetrics.fontMeta
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                    }

                    Text
                    {
                        text: qsTr("Rare words")
                        font.pixelSize: AppMetrics.fontMeta
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                    }
                }
            }

            Item { Layout.fillHeight: true }

            MobileWideButton
            {
                objectName: "placement_new"

                visible: root.phase === "intro"
                label: qsTr("I'm new to %1").arg(root.languageName)
                filled: false

                onClicked: root.chooseNew()
            }

            Repeater
            {
                model: root.phase === "intro" ? root.levels : []

                delegate: MobileWideButton
                {
                    required property var modelData
                    required property int index

                    objectName: modelData.name
                    label: modelData.label
                    filled: false

                    onClicked: root.begin(index + 1)
                }
            }

            MobileWideButton
            {
                objectName: "placement_cancel"

                visible: root.phase === "intro" && root.retake
                label: qsTr("Cancel")
                filled: false

                onClicked: root.leave()
            }

            MobileWideButton
            {
                objectName: "placement_continue"

                visible: root.showsResult
                label: qsTr("Continue")

                onClicked: root.leave()
            }
        }
    }

    MobileCorrectIncorrectScreen
    {
        anchors.fill: parent
        visible: root.phase === "test"

        controller: root.controller
        showComplete: false
        showMute: false
        showOptions: false

        exerciseName: qsTr("Placement test")
        incorrectLabel: qsTr("I did not know it")
        correctLabel: qsTr("I knew it")

        onDone: root.phase = "intro"
    }
}
