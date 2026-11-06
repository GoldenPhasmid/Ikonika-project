import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var forms
    property string headword: ""
    property bool headwordIsFeminine: false

    readonly property var moodNames: ["", qsTr("Indicative"), qsTr("Subjunctive"),
        qsTr("Conditional"), qsTr("Imperative")]
    readonly property var tenseNames: [qsTr("Present"), qsTr("Present"), qsTr("Imperfect"),
        root.forms.language === "en" ? qsTr("Simple past") : qsTr("Historic"), qsTr("Future")]
    readonly property var nonfiniteNames: ["", "", "", "", "", qsTr("Infinitive"), qsTr("Gerund"),
        qsTr("Present participle"), qsTr("Past participle")]
    readonly property var personNames: root.forms.language === "en" ? root.forms.cues : [qsTr("je"), qsTr("tu"), qsTr("il/elle"), qsTr("nous"),
        qsTr("vous"), qsTr("ils/elles")]
    readonly property var genderNames: [qsTr("Masculine"), qsTr("Feminine")]
    readonly property var numberNames: [qsTr("Singular"), qsTr("Plural")]

    property int currentMoodIndex: 0

    Layout.fillWidth: true

    spacing: AppMetrics.gapSmall

    visible: forms.kind !== "none"

    onFormsChanged: currentMoodIndex = 0

    Flow
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapMedium
        visible: root.forms.kind === "verb" && root.forms.nonfinite.length > 0

        Repeater
        {
            model: root.forms.kind === "verb" ? root.forms.nonfinite : []

            delegate: RowLayout
            {
                required property var modelData

                spacing: AppMetrics.gapTight

                Text
                {
                    wrapMode: Text.NoWrap
                    text: modelData.text
                    color: AppTheme.colourText
                    font.pixelSize: AppMetrics.fontSmall
                    font.family: AppMetrics.fontFamily
                }

                MobileDictionaryFormsLabel { text: root.nonfiniteNames[modelData.tense] }
            }
        }
    }

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall
        visible: root.forms.kind === "verb"

        Repeater
        {
            model: root.forms.kind === "verb" ? root.forms.moods : []

            delegate: MobileSessionButton
            {
                required property var modelData
                required property int index

                Layout.fillWidth: true

                label: root.moodNames[modelData.mood]
                filled: root.currentMoodIndex === index

                onClicked: root.currentMoodIndex = index
            }
        }
    }

    MobileCard
    {
        id: verbCard

        Layout.fillWidth: true

        visible: root.forms.kind === "verb" && root.forms.moods.length > 0

        readonly property var selectedMood:
            root.forms.kind === "verb" && root.currentMoodIndex < root.forms.moods.length
                ? root.forms.moods[root.currentMoodIndex] : undefined

        readonly property var flatCells:
        {
            if (!selectedMood)
            {
                return []
            }

            const tenses = selectedMood.tenses
            const flat = []

            for (let person = 0; person < root.personNames.length; ++person)
            {
                flat.push(root.personNames[person])
                for (let tense = 0; tense < tenses.length; ++tense)
                {
                    flat.push(tenses[tense].cells[person])
                }
            }

            return flat
        }

        GridLayout
        {
            id: verbTable

            Layout.fillWidth: true
            columns: 1 + (verbCard.selectedMood ? verbCard.selectedMood.tenses.length : 1)
            rowSpacing: AppMetrics.gapTight
            columnSpacing: AppMetrics.gapMedium

            Item { Layout.preferredWidth: 1 }

            Repeater
            {
                model: verbCard.selectedMood ? verbCard.selectedMood.tenses : []

                delegate: MobileDictionaryFormsLabel
                {
                    required property var modelData

                    Layout.fillWidth: true

                    text: root.tenseNames[modelData.tense]
                }
            }

            Rectangle
            {
                Layout.columnSpan: verbTable.columns
                Layout.fillWidth: true
                Layout.preferredHeight: AppMetrics.borderWidth
                color: AppTheme.colourBorder
            }

            Repeater
            {
                model: verbCard.flatCells

                delegate: Text
                {
                    required property string modelData
                    required property int index

                    readonly property bool isPerson: index % verbTable.columns === 0

                    Layout.fillWidth: !isPerson

                    wrapMode: Text.NoWrap
                    text: isPerson ? modelData : (modelData.length > 0 ? modelData : "—")
                    color: isPerson ? AppTheme.colourMuted : AppTheme.colourText
                    font.pixelSize: isPerson ? AppMetrics.fontMeta : AppMetrics.fontSmall
                    font.family: AppMetrics.fontFamily
                    font.letterSpacing: isPerson ? AppMetrics.trackingWide : 0
                    font.capitalization: isPerson ? Font.AllUppercase : Font.MixedCase
                }
            }
        }
    }

    MobileCard
    {
        Layout.fillWidth: true

        visible: root.forms.kind === "agreement"

        GridLayout
        {
            id: agreementTable

            Layout.fillWidth: true
            columns: 3
            rowSpacing: AppMetrics.gapSmall
            columnSpacing: AppMetrics.gapMedium

            function cellText(gender, number)
            {
                if (root.forms.kind !== "agreement")
                {
                    return "—"
                }

                const row = root.forms.cells[gender]
                const value = row ? row[number] : undefined

                if (value && value.length > 0)
                {
                    return value
                }

                const isHeadwordCell = number === 0
                    && gender === (root.headwordIsFeminine ? 1 : 0)

                return isHeadwordCell ? root.headword : "—"
            }

            function hasGenderRow(gender)
            {
                if (gender === (root.headwordIsFeminine ? 1 : 0))
                {
                    return true
                }

                if (root.forms.kind !== "agreement")
                {
                    return false
                }

                const row = root.forms.cells[gender]

                return row && ((row[0] && row[0].length > 0) || (row[1] && row[1].length > 0))
            }

            Item { Layout.preferredWidth: 1 }
            MobileDictionaryFormsLabel { Layout.fillWidth: true; text: root.numberNames[0] }
            MobileDictionaryFormsLabel { Layout.fillWidth: true; text: root.numberNames[1] }

            Rectangle
            {
                Layout.columnSpan: 3
                Layout.fillWidth: true
                Layout.preferredHeight: AppMetrics.borderWidth
                color: AppTheme.colourBorder
            }

            MobileDictionaryFormsLabel
            {
                visible: agreementTable.hasGenderRow(0)
                text: root.genderNames[0]
            }
            Text
            {
                visible: agreementTable.hasGenderRow(0)
                Layout.fillWidth: true
                wrapMode: Text.NoWrap
                text: parent.cellText(0, 0)
                color: AppTheme.colourText
                font.pixelSize: AppMetrics.fontSmall
                font.family: AppMetrics.fontFamily
            }
            Text
            {
                visible: agreementTable.hasGenderRow(0)
                Layout.fillWidth: true
                wrapMode: Text.NoWrap
                text: parent.cellText(0, 1)
                color: AppTheme.colourText
                font.pixelSize: AppMetrics.fontSmall
                font.family: AppMetrics.fontFamily
            }

            MobileDictionaryFormsLabel
            {
                visible: agreementTable.hasGenderRow(1)
                text: root.genderNames[1]
            }
            Text
            {
                visible: agreementTable.hasGenderRow(1)
                Layout.fillWidth: true
                wrapMode: Text.NoWrap
                text: parent.cellText(1, 0)
                color: AppTheme.colourText
                font.pixelSize: AppMetrics.fontSmall
                font.family: AppMetrics.fontFamily
            }
            Text
            {
                visible: agreementTable.hasGenderRow(1)
                Layout.fillWidth: true
                wrapMode: Text.NoWrap
                text: parent.cellText(1, 1)
                color: AppTheme.colourText
                font.pixelSize: AppMetrics.fontSmall
                font.family: AppMetrics.fontFamily
            }
        }
    }
}
