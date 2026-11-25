import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ReaderTranslationBase
{
    id: root

    property bool darkPaper: false
    property real contentWidth: root.width
    property bool selecting: false
    property bool showTrigger: true
    property bool pending: false

    readonly property bool available: root.brief || root.lookup.machinePack.installed || root.lookup.machinePack.offered

    function expect()
    {
        root.pending = true
    }

    function containsPopup(position)
    {
        return box.visible && box.contains(box.mapFromItem(root, position))
    }

    onShown: root.pending = false

    DesktopButton
    {
        id: trigger

        objectName: "reader_translate"

        visible: root.showTrigger && !root.open && !root.pending && !root.selecting && root.onScreen && root.available
        x: Math.max(AppMetrics.gapSmall, Math.min(root.width - trigger.width - AppMetrics.gapSmall, root.geometry ? root.geometry.endX : 0))
        y: root.geometry ? (root.geometry.bottom + trigger.height + AppMetrics.gapSmall <= root.height
            ? root.geometry.bottom + AppMetrics.gapSmall
            : Math.max(AppMetrics.gapSmall, root.geometry.top - trigger.height - AppMetrics.gapSmall)) : 0
        focusPolicy: Qt.NoFocus
        iconName: "languages"
        text: qsTr("Translate")

        background: Rectangle
        {
            radius: AppMetrics.radiusMedium
            color: AppTheme.colourPanel
            border.width: AppMetrics.borderWidth
            border.color: AppTheme.colourBorder

            Rectangle
            {
                anchors.fill: parent
                anchors.margins: AppMetrics.borderWidth
                radius: parent.radius - AppMetrics.borderWidth
                color: trigger.down ? AppTheme.colourPressed : (trigger.hovered ? AppTheme.colourHover : "transparent")

                Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }
            }
        }

        onClicked: root.show()
    }

    Rectangle
    {
        id: box

        objectName: "reader_translation"

        readonly property real below: root.geometry ? root.geometry.bottom + AppMetrics.gapSmall : 0
        readonly property real above: root.geometry ? root.geometry.top - AppMetrics.gapSmall - box.height : 0
        readonly property real minimumWidth: root.brief ? AppMetrics.popoverWidth
                                                        : root.contentWidth * AppMetrics.readerTranslationWidthShare
        readonly property real iconInset: (AppMetrics.iconSize - AppMetrics.iconTapSize) / 2
        readonly property real cardInset: AppMetrics.gapTight - AppMetrics.gapMedium
        readonly property bool bare: root.downloadShown && !root.lookup.machinePack.downloading
        readonly property real padding: box.bare ? 0 : AppMetrics.gapMedium

        visible: root.open && root.onScreen && !root.searching
        width: box.bare ? download.fullWidth
                        : Math.min(root.width - AppMetrics.gapMedium * 2,
                                   Math.max(box.minimumWidth, root.brief || !root.geometry ? 0 : root.geometry.width))
        height: content.implicitHeight + box.padding * 2
        x: Math.max(AppMetrics.gapMedium, Math.min(root.width - box.width - AppMetrics.gapMedium, root.geometry ? root.geometry.left : 0))
        y: box.below + box.height <= root.height - AppMetrics.gapMedium || box.above < AppMetrics.gapMedium ? box.below : box.above

        radius: AppMetrics.radiusLarge
        color: box.bare ? "transparent" : root.darkPaper ? AppTheme.colourCard : AppTheme.colourPanel
        border.width: box.bare ? 0 : AppMetrics.borderWidth
        border.color: root.darkPaper ? AppTheme.colourOutline : AppTheme.colourBorder

        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Translation")

        onVisibleChanged: if (box.visible) entrance.restart()

        ParallelAnimation
        {
            id: entrance

            NumberAnimation { target: box; property: "opacity"; from: 0; to: 1; duration: AppMetrics.motionFast }
            NumberAnimation { target: rise; property: "y"; from: -AppMetrics.gapTight; to: 0; duration: AppMetrics.motionFast; easing.type: Easing.OutCubic }
        }

        transform: Translate { id: rise }

        MouseArea
        {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        ColumnLayout
        {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: box.padding
            spacing: AppMetrics.gapSmall

            ColumnLayout
            {
                Layout.fillWidth: true
                visible: root.cardShown
                spacing: 0

                Repeater
                {
                    model: root.shownMatches

                    delegate: DesktopWordRow
                    {
                        required property var modelData
                        required property int index

                        objectName: "reader_translation_card_" + index

                        Layout.fillWidth: true
                        Layout.leftMargin: box.cardInset
                        Layout.rightMargin: box.cardInset
                        Layout.topMargin: index === 0 ? box.cardInset : 0

                        word: modelData.displayWord || modelData.word
                        translation: modelData.translation || ""
                        partOfSpeech: root.shownMatches.length > 1 ? (modelData.partOfSpeechLabel || "") : ""
                        verbGroup: modelData.verbGroup || ""
                        stage: modelData.stage !== undefined ? modelData.stage : -1
                        completed: !!modelData.completed
                        maxStage: root.maxStage

                        onClicked: root.addMatch(modelData)
                    }
                }
            }

            DesktopMachineTranslation
            {
                objectName: "reader_translation_form"

                lookup: root.lookup
                term: root.text
                rowInset: box.cardInset
                visible: root.cardShown && root.lookup.formTerm.length > 0

                onAddRequested: function(word, translation)
                {
                    root.addWordRequested(word, translation, "phrase", root.sentence)
                }
            }

            DesktopLabel
            {
                objectName: "reader_translation_text"

                Layout.fillWidth: true
                visible: !root.cardShown && !root.downloadShown
                text: root.lookup.translating ? qsTr("Translating")
                    : root.translated ? root.lookup.machineTranslation
                    : root.brief ? qsTr("Not in the dictionary") : qsTr("No translation")
                color: root.translated ? AppTheme.colourText : AppTheme.colourMuted
                font.pixelSize: AppMetrics.fontLead
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }

            DesktopTranslationDownload
            {
                id: download

                source: root.lookup.machinePack
                buttonName: "reader_translation_download"

                Layout.fillWidth: true
                visible: root.downloadShown
            }

            RowLayout
            {
                Layout.fillWidth: true
                Layout.leftMargin: box.iconInset + (root.cardShown ? AppMetrics.gapTight : 0)
                Layout.topMargin: box.iconInset
                Layout.bottomMargin: box.iconInset
                visible: root.actionsShown
                spacing: 0

                DesktopIconButton
                {
                    objectName: "reader_translation_listen"

                    focusPolicy: Qt.NoFocus
                    iconName: "speaker_on"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Listen")

                    onClicked: root.lookup.speak(root.cardShown ? root.match.word : root.text)
                }

                DesktopIconButton
                {
                    objectName: "reader_translation_read_aloud"

                    visible: root.readAloudOffered
                    focusPolicy: Qt.NoFocus
                    iconName: "headphones"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Read aloud from here")

                    onClicked: root.readAloudRequested()
                }

                DesktopIconButton
                {
                    objectName: "reader_translation_copy"

                    visible: !root.cardShown && root.translated
                    focusPolicy: Qt.NoFocus
                    iconName: "copy"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Copy translation")

                    onClicked: root.lookup.copyToClipboard(root.lookup.machineTranslation)
                }

                DesktopIconButton
                {
                    objectName: "reader_translation_dictionary"

                    visible: root.cardShown
                    focusPolicy: Qt.NoFocus
                    iconName: "nav_words"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Open in dictionary")

                    onClicked: root.dictionaryRequested(root.match.word)
                }

                DesktopIconButton
                {
                    objectName: "reader_translation_add"

                    visible: root.brief || root.translated
                    focusPolicy: Qt.NoFocus
                    iconName: "plus"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Add to My Vocabulary")

                    onClicked: root.addWord()
                }

                DesktopIconButton
                {
                    objectName: "reader_translation_known"

                    visible: root.knowable
                    focusPolicy: Qt.NoFocus
                    iconName: root.known ? "circle_check" : "check"
                    iconColour: root.known ? AppTheme.colourAccent : AppTheme.colourMuted
                    text: root.known ? qsTr("Known. Click to undo") : qsTr("I know this word")

                    onClicked: root.toggleKnown()
                }

                Item
                {
                    Layout.fillWidth: true
                }

                DesktopLabel
                {
                    Layout.rightMargin: -box.iconInset
                    visible: root.translated && !root.cardShown && AppMachineTranslation.attribution.length > 0
                    text: AppMachineTranslation.attribution
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourSubtle
                    elide: Text.ElideRight
                }
            }
        }
    }
}
