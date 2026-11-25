import QtQuick
import QtQuick.Layouts
import "../ReaderTones.js" as Tones

ColumnLayout
{
    id: root

    required property var optionPicker

    property bool showPdf: true
    property bool showText: true
    property bool titled: true
    property ReaderReadAloud readAloud: null

    readonly property var pageTones: Tones.list()
    readonly property var pageTone: Tones.find(AppReaderSettings.pageTone)
    readonly property var textTones: Tones.textList()
    readonly property var textTone: Tones.findText(AppReaderSettings.textTone)
    readonly property var fontChoices: [AppMetrics.fontFamily].concat(AppReaderSettings.textFonts)
    readonly property bool volumeKeysAvailable: Qt.platform.os === "android"
    readonly property string volumeKeysPlace: root.titled || !root.volumeKeysAvailable ? ""
                                                                                      : root.showText ? "text" : "pdf"
    readonly property string fontChoice: AppReaderSettings.textFont.length > 0 ? AppReaderSettings.textFont
                                                                               : AppMetrics.fontFamily

    Layout.fillWidth: true
    spacing: root.titled ? AppMetrics.gapSmall : AppMetrics.gapLarge

    component PdfColourSelect: MobileSettingsSelect
    {
        id: pdfColourSelect

        required property var owner

        label: pdfColourSelect.owner.pageTone.label

        onClicked: pdfColourSelect.owner.optionPicker.open(qsTr("Page colour"),
            pdfColourSelect.owner.pageTones.map(function(tone) { return tone.label }),
            pdfColourSelect.owner.pageTones.findIndex(function(tone) { return tone.key === pdfColourSelect.owner.pageTone.key }),
            function(index) { AppReaderSettings.pageTone = pdfColourSelect.owner.pageTones[index].key })
    }

    component TextColourSelect: MobileSettingsSelect
    {
        id: textColourSelect

        required property var owner

        objectName: "reader_text_colour"

        label: textColourSelect.owner.textTone.label

        onClicked: textColourSelect.owner.optionPicker.open(qsTr("Page colour"),
            textColourSelect.owner.textTones.map(function(tone) { return tone.label }),
            textColourSelect.owner.textTones.findIndex(function(tone) { return tone.key === textColourSelect.owner.textTone.key }),
            function(index) { AppReaderSettings.textTone = textColourSelect.owner.textTones[index].key })
    }

    component FontSelect: MobileSettingsSelect
    {
        id: fontSelect

        required property var owner

        objectName: "reader_font"

        label: fontSelect.owner.fontChoice

        onClicked: fontSelect.owner.optionPicker.open(qsTr("Font"),
            fontSelect.owner.fontChoices,
            fontSelect.owner.fontChoices.indexOf(fontSelect.owner.fontChoice),
            function(index) { AppReaderSettings.textFont = index > 0 ? AppReaderSettings.textFonts[index - 1] : "" })
    }

    component SentenceSpeedSlider: MobileSettingsSlider
    {
        id: sentenceSpeedSlider

        required property ReaderReadAloud readAloud

        objectName: "reader_sentence_speed"

        title: qsTr("Sentence speech rate")
        value: sentenceSpeedSlider.readAloud !== null ? Math.round(sentenceSpeedSlider.readAloud.speed * 100) : 100
        from: sentenceSpeedSlider.readAloud !== null ? Math.round(sentenceSpeedSlider.readAloud.minimumSpeed * 100) : 50
        to: sentenceSpeedSlider.readAloud !== null ? Math.round(sentenceSpeedSlider.readAloud.maximumSpeed * 100) : 150
        step: 5
        suffix: "%"

        onEdited: function(newValue) { sentenceSpeedSlider.readAloud.speed = newValue / 100 }
    }

    component HighlightPill: MobileSegmentedPill
    {
        objectName: "reader_highlight_mode"

        Layout.fillWidth: true
        options: [qsTr("Off"), qsTr("Vocabulary"), qsTr("New words")]
        currentIndex: !AppReaderSettings.highlightVocabulary ? 0 : (AppReaderSettings.highlightUnknownWords ? 2 : 1)

        onChosen: function(index)
        {
            AppReaderSettings.highlightVocabulary = index > 0
            AppReaderSettings.highlightUnknownWords = index === 2
        }
    }

    MobileSectionLabel
    {
        visible: root.titled && root.showPdf
        text: qsTr("PDF")
    }

    MobileOptionListPanel
    {
        visible: root.showPdf

        MobileSettingsWideRow
        {
            visible: !root.titled

            PdfColourSelect
            {
                owner: root

                Layout.fillWidth: true
            }
        }

        MobileSettingsRow
        {
            visible: root.titled
            title: qsTr("Page colour")

            PdfColourSelect
            {
                owner: root
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Page margins")
            lastRow: root.volumeKeysPlace !== "pdf" && (root.readAloud === null || root.showText)

            MobileSettingsToggle
            {
                checked: !AppReaderSettings.cropMargins

                onToggled: AppReaderSettings.cropMargins = !AppReaderSettings.cropMargins
            }
        }

        SentenceSpeedSlider
        {
            readAloud: root.readAloud
            visible: root.readAloud !== null && !root.showText
            lastRow: root.volumeKeysPlace !== "pdf"
        }

        MobileSettingsVolumeKeysRow
        {
            visible: root.volumeKeysPlace === "pdf"
            lastRow: true
        }
    }

    MobileSectionLabel
    {
        visible: root.titled && root.showText
        Layout.topMargin: root.showPdf ? AppMetrics.gapMedium : 0
        text: qsTr("EPUB, FB2, TXT")
    }

    MobileOptionListPanel
    {
        visible: root.showText

        MobileSettingsWideRow
        {
            visible: !root.titled

            TextColourSelect
            {
                owner: root

                Layout.fillWidth: true
                Layout.preferredWidth: 1
            }

            FontSelect
            {
                owner: root

                Layout.fillWidth: true
                Layout.preferredWidth: 1
            }
        }

        MobileSettingsRow
        {
            visible: root.titled
            title: qsTr("Page colour")

            TextColourSelect
            {
                owner: root
            }
        }

        MobileSettingsRow
        {
            visible: root.titled
            title: qsTr("Font")

            FontSelect
            {
                owner: root
            }
        }

        MobileSettingsRow
        {
            visible: root.titled
            title: qsTr("Word highlighting")
            lastRow: true
        }

        MobileSettingsWideRow
        {
            HighlightPill {}
        }

        SentenceSpeedSlider
        {
            readAloud: root.readAloud
            visible: root.readAloud !== null && root.showText
        }

        MobileSettingsSlider
        {
            title: qsTr("Text size")
            value: AppReaderSettings.textSize
            from: AppReaderSettings.minimumTextSize
            to: AppReaderSettings.maximumTextSize
            valueText: AppReaderSettings.textSize + " px"

            onEdited: function(newValue) { AppReaderSettings.textSize = newValue }
        }

        MobileSettingsSlider
        {
            title: qsTr("Line spacing")
            value: AppReaderSettings.lineSpacing
            from: AppReaderSettings.minimumLineSpacing
            to: AppReaderSettings.maximumLineSpacing
            step: 10
            valueText: AppReaderSettings.lineSpacing + "%"

            onEdited: function(newValue) { AppReaderSettings.lineSpacing = newValue }
        }

        MobileSettingsSlider
        {
            title: qsTr("Margins")
            value: AppReaderSettings.textMargin
            from: AppReaderSettings.minimumTextMargin
            to: AppReaderSettings.maximumTextMargin
            step: 4
            valueText: AppReaderSettings.textMargin + " px"
            lastRow: root.volumeKeysPlace !== "text"

            onEdited: function(newValue) { AppReaderSettings.textMargin = newValue }
        }

        MobileSettingsVolumeKeysRow
        {
            visible: root.volumeKeysPlace === "text"
            lastRow: true
        }
    }

    MobileSectionLabel
    {
        visible: root.titled && root.volumeKeysAvailable
        Layout.topMargin: AppMetrics.gapMedium
        text: qsTr("KEYS")
    }

    MobileOptionListPanel
    {
        visible: root.titled && root.volumeKeysAvailable

        MobileSettingsVolumeKeysRow
        {
            lastRow: true
        }
    }
}
