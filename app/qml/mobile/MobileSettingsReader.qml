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

    readonly property var pageTones: Tones.list()
    readonly property var pageTone: Tones.find(AppReaderSettings.pageTone)
    readonly property var textTones: Tones.textList()
    readonly property var textTone: Tones.findText(AppReaderSettings.textTone)
    readonly property var fontChoices: [AppMetrics.fontFamily].concat(AppReaderSettings.textFonts)
    readonly property string fontChoice: AppReaderSettings.textFont.length > 0 ? AppReaderSettings.textFont
                                                                               : AppMetrics.fontFamily

    Layout.fillWidth: true
    spacing: root.titled ? AppMetrics.gapSmall : AppMetrics.gapLarge

    MobileSectionLabel
    {
        visible: root.titled && root.showPdf
        text: qsTr("PDF")
    }

    MobileOptionListPanel
    {
        visible: root.showPdf

        MobileSettingsRow
        {
            title: qsTr("Page colour")

            MobileSettingsSelect
            {
                label: root.pageTone.label

                onClicked: root.optionPicker.open(qsTr("Page colour"),
                    root.pageTones.map(function(tone) { return tone.label }),
                    root.pageTones.findIndex(function(tone) { return tone.key === root.pageTone.key }),
                    function(index) { AppReaderSettings.pageTone = root.pageTones[index].key })
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Page margins")
            lastRow: true

            MobileSettingsToggle
            {
                checked: !AppReaderSettings.cropMargins

                onToggled: AppReaderSettings.cropMargins = !AppReaderSettings.cropMargins
            }
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

        MobileSettingsRow
        {
            title: qsTr("Page colour")

            MobileSettingsSelect
            {
                objectName: "reader_text_colour"

                label: root.textTone.label

                onClicked: root.optionPicker.open(qsTr("Page colour"),
                    root.textTones.map(function(tone) { return tone.label }),
                    root.textTones.findIndex(function(tone) { return tone.key === root.textTone.key }),
                    function(index) { AppReaderSettings.textTone = root.textTones[index].key })
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Font")

            MobileSettingsSelect
            {
                objectName: "reader_font"

                label: root.fontChoice

                onClicked: root.optionPicker.open(qsTr("Font"),
                    root.fontChoices,
                    root.fontChoices.indexOf(root.fontChoice),
                    function(index) { AppReaderSettings.textFont = index > 0 ? AppReaderSettings.textFonts[index - 1] : "" })
            }
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

            onEdited: function(newValue) { AppReaderSettings.textMargin = newValue }
        }

        MobileSettingsRow
        {
            title: qsTr("Highlight vocabulary")
            lastRow: true

            MobileSettingsToggle
            {
                objectName: "reader_highlight_vocabulary"

                checked: AppReaderSettings.highlightVocabulary

                onToggled: AppReaderSettings.highlightVocabulary = !AppReaderSettings.highlightVocabulary
            }
        }
    }

    MobileSectionLabel
    {
        visible: root.titled && Qt.platform.os === "android"
        Layout.topMargin: AppMetrics.gapMedium
        text: qsTr("KEYS")
    }

    MobileOptionListPanel
    {
        visible: Qt.platform.os === "android"

        MobileSettingsRow
        {
            title: qsTr("Volume keys turn pages")
            lastRow: true

            MobileSettingsToggle
            {
                checked: AppReaderSettings.volumeKeysTurnPages

                onToggled: AppReaderSettings.volumeKeysTurnPages = !AppReaderSettings.volumeKeysTurnPages
            }
        }
    }
}
