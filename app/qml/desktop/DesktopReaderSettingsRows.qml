import QtQuick
import QtQuick.Layouts
import "../ReaderTones.js" as Tones

ColumnLayout
{
    id: root

    property bool showPdf: true
    property bool showText: true
    property real inset: AppMetrics.gapLarge
    property ReaderReadAloud readAloud: null

    readonly property var pageTones: root.showPdf ? Tones.list() : Tones.textList()
    readonly property string pageToneKey: root.showPdf ? AppReaderSettings.pageTone : AppReaderSettings.textTone
    readonly property int pageToneIndex: Math.max(0, root.pageTones.findIndex(Tone => Tone.key === root.pageToneKey))

    Layout.fillWidth: true
    spacing: 0

    DesktopSettingsRow
    {
        leftPadding: root.inset
        rightPadding: root.inset
        text: qsTr("Page colour")

        DesktopSelect
        {
            objectName: "reader_page_colour"

            Layout.fillWidth: false
            heading: qsTr("Page colour")
            options: root.pageTones.map(Tone => Tone.label)
            currentIndex: root.pageToneIndex

            onChosen: function(index)
            {
                if (root.showPdf)
                {
                    AppReaderSettings.pageTone = root.pageTones[index].key

                    return
                }

                AppReaderSettings.textTone = root.pageTones[index].key
            }
        }
    }

    DesktopSettingsRow
    {
        visible: root.showPdf
        leftPadding: root.inset
        rightPadding: root.inset
        text: qsTr("Fit to text width")
        lastRow: !root.showText

        DesktopToggle
        {
            objectName: "reader_crop_margins"

            accessibleName: qsTr("Fit to text width")
            on: AppReaderSettings.cropMargins

            onClicked: AppReaderSettings.cropMargins = !AppReaderSettings.cropMargins
        }
    }

    DesktopSettingsRow
    {
        visible: root.showText
        leftPadding: root.inset
        rightPadding: root.inset
        text: qsTr("Font")

        DesktopSelect
        {
            objectName: "reader_font"

            Layout.fillWidth: false
            heading: qsTr("Font")
            options: [AppMetrics.fontFamily].concat(AppReaderSettings.textFonts)
            currentIndex: AppReaderSettings.textFont.length > 0 ? AppReaderSettings.textFonts.indexOf(AppReaderSettings.textFont) + 1 : 0
            visibleRows: 8

            onChosen: function(index) { AppReaderSettings.textFont = index > 0 ? AppReaderSettings.textFonts[index - 1] : "" }
        }
    }

    DesktopSettingsSlider
    {
        visible: root.showText
        inset: root.inset
        title: qsTr("Text size")
        value: AppReaderSettings.textSize
        from: AppReaderSettings.minimumTextSize
        to: AppReaderSettings.maximumTextSize
        valueText: qsTr("%1 px").arg(AppReaderSettings.textSize)

        onEdited: function(newValue) { AppReaderSettings.textSize = newValue }
    }

    DesktopSettingsSlider
    {
        visible: root.showText
        inset: root.inset
        title: qsTr("Line spacing")
        value: AppReaderSettings.lineSpacing
        from: AppReaderSettings.minimumLineSpacing
        to: AppReaderSettings.maximumLineSpacing
        step: 10
        valueText: qsTr("%1%").arg(AppReaderSettings.lineSpacing)

        onEdited: function(newValue) { AppReaderSettings.lineSpacing = newValue }
    }

    DesktopSettingsSlider
    {
        visible: root.showText
        inset: root.inset
        title: qsTr("Page width")
        value: AppReaderSettings.pageWidth
        from: AppReaderSettings.minimumPageWidth
        to: AppReaderSettings.maximumPageWidth
        step: 10
        valueText: qsTr("%1%").arg(AppReaderSettings.pageWidth)

        onEdited: function(newValue) { AppReaderSettings.pageWidth = newValue }
    }

    DesktopSettingsSlider
    {
        visible: root.showText
        inset: root.inset
        title: qsTr("Margins")
        value: AppReaderSettings.textMargin
        from: AppReaderSettings.minimumTextMargin
        to: AppReaderSettings.maximumTextMargin
        step: 4
        valueText: qsTr("%1 px").arg(AppReaderSettings.textMargin)

        onEdited: function(newValue) { AppReaderSettings.textMargin = newValue }
    }

    DesktopSettingsSlider
    {
        objectName: "reader_sentence_speed"

        visible: root.showText && root.readAloud !== null
        inset: root.inset
        title: qsTr("Sentence speech rate")
        value: root.readAloud !== null ? Math.round(root.readAloud.speed * 100) : 100
        from: root.readAloud !== null ? Math.round(root.readAloud.minimumSpeed * 100) : 50
        to: root.readAloud !== null ? Math.round(root.readAloud.maximumSpeed * 100) : 150
        step: 5
        suffix: "%"

        onEdited: function(newValue) { root.readAloud.speed = newValue / 100 }
    }

    DesktopSettingsRow
    {
        visible: root.showText
        leftPadding: root.inset
        rightPadding: root.inset
        text: qsTr("Highlight vocabulary")
        lastRow: !AppReaderSettings.highlightVocabulary

        DesktopToggle
        {
            objectName: "reader_highlight_vocabulary"

            accessibleName: qsTr("Highlight vocabulary")
            on: AppReaderSettings.highlightVocabulary

            onClicked: AppReaderSettings.highlightVocabulary = !AppReaderSettings.highlightVocabulary
        }
    }

    DesktopSettingsRow
    {
        visible: root.showText && AppReaderSettings.highlightVocabulary
        leftPadding: root.inset
        rightPadding: root.inset
        text: qsTr("Highlight unknown words")
        lastRow: true

        DesktopToggle
        {
            objectName: "reader_highlight_unknown_words"
            accessibleName: qsTr("Highlight unknown words")
            on: AppReaderSettings.highlightUnknownWords
            onClicked: AppReaderSettings.highlightUnknownWords = !AppReaderSettings.highlightUnknownWords
        }
    }
}
