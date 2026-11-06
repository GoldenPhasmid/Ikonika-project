import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property var forms: ({ kind: "none" })
    property string chosenKey: ""
    property string conjugationUrl: ""

    onFormsChanged: root.chosenKey = root.forms.matchedKey || ""

    readonly property string matchDescription:
    {
        if (!root.forms.matchedText)
        {
            return ""
        }
        const names = root.tenses.filter(tense => tense.matched).map(tense => tense.description)
        for (const form of root.participles)
        {
            if (form.matched)
            {
                names.push(form.label)
            }
        }
        return root.forms.matchedText + " · " + names.join(" / ")
    }

    readonly property bool verb: root.forms.kind === "verb"
    readonly property var cues: root.forms.cues || []

    readonly property int tenseColumns: root.width >=
        (tenseLabelMetrics.advanceWidth + AppMetrics.gapCompact * 2) * 4 + AppMetrics.gapSmall * 3 ? 4 : 2

    TextMetrics
    {
        id: tenseLabelMetrics
        text: root.subjunctiveNames.concat(root.tenseNames).reduce((longest, label) => label.length > longest.length ? label : longest, "")
        font.family: AppMetrics.fontFamily
        font.pixelSize: AppMetrics.fontSmall
        font.weight: Font.Medium
    }

    readonly property var tenseNames: ["", qsTr("Present"), qsTr("Imperfect"), qsTr("Simple past"), qsTr("Future")]
    readonly property var subjunctiveNames: ["", qsTr("Subjunctive"), qsTr("Imperfect subjunctive"), "",
        qsTr("Future subjunctive")]
    readonly property var nonfiniteNames: ["", "", "", "", "", "", qsTr("Gerund"), qsTr("Present participle"),
        qsTr("Past participle")]

    readonly property var tenses:
    {
        if (!root.verb)
        {
            return []
        }

        const listed = []
        let place = 0

        for (const mood of root.forms.moods)
        {
            for (const tense of mood.tenses)
            {
                listed.push({ key: mood.mood + "-" + tense.tense, label: root.nameOf(mood.mood, tense.tense),
                              marked: tense.marked, matches: tense.matches || [], matched: !!tense.matched,
                              description: (mood.mood === 3 ? qsTr("Present conditional") : mood.mood === 4 ? qsTr("Present imperative") : mood.mood === 2 && tense.tense === 1 ? qsTr("Present subjunctive") : root.nameOf(mood.mood, tense.tense)), row: Math.floor(place / root.tenseColumns),
                              column: place % root.tenseColumns })
                ++place
            }

            if (mood.mood === 1)
            {
                place = Math.ceil(place / root.tenseColumns) * root.tenseColumns
            }
        }

        return listed
    }

    readonly property var shownTense: root.tenses.find(Tense => Tense.key === root.chosenKey) || root.tenses[0] || null

    readonly property var rows:
    {
        if (root.verb)
        {
            if (!root.shownTense)
            {
                return []
            }

            const perNumber = root.shownTense.marked.length / 2
            const listed = []

            for (let person = 0; person < perNumber; ++person)
            {
                listed.push({ cells: [root.cell(person, root.shownTense.marked[person], root.shownTense.matches[person]),
                                      root.cell(person + perNumber, root.shownTense.marked[person + perNumber], root.shownTense.matches[person + perNumber])] })
            }

            return listed
        }

        if (root.forms.kind !== "agreement")
        {
            return []
        }

        const listed = []

        for (let gender = 0; gender < root.forms.marked.length; ++gender)
        {
            const marked = root.forms.marked[gender]

            if (marked[0].length > 0 || marked[1].length > 0)
            {
                listed.push({ cells: [root.cell(gender, marked[0]), root.cell(gender + 2, marked[1])] })
            }
        }

        return listed
    }

    readonly property var participles:
    {
        if (!root.verb)
        {
            return []
        }

        const listed = []

        for (const form of root.forms.nonfinite)
        {
            const name = root.nonfiniteNames[form.tense] || ""
            const repeated = listed.find(Listed => Listed.text === form.text)

            if (name.length === 0)
            {
                continue
            }

            if (repeated)
            {
                repeated.label = name

                continue
            }

            listed.push({ label: name, text: form.text, matched: !!form.matched })
        }

        return listed
    }

    function nameOf(mood, tense)
    {
        if (mood === 2)
        {
            return root.subjunctiveNames[tense] || qsTr("Subjunctive")
        }

        if (mood === 3)
        {
            return qsTr("Conditional")
        }

        if (mood === 4)
        {
            return qsTr("Imperative")
        }

        return root.tenseNames[tense] || qsTr("Present")
    }

    function cell(cueIndex, form, matched)
    {
        return { cue: root.cues[cueIndex] || "", form: form, matched: !!matched }
    }

    Layout.fillWidth: true
    spacing: AppMetrics.gapCompact
    visible: root.rows.length > 0

    RowLayout
    {
        Layout.fillWidth: true
        DesktopSectionLabel { Layout.fillWidth: true; text: root.verb ? qsTr("Conjugation") : qsTr("Forms") }
        DesktopButton
        {
            objectName: "dictionary_reverso"
            visible: root.verb && root.conjugationUrl.length > 0
            text: qsTr("Reverso")
            iconName: "external_link"
            Accessible.name: qsTr("Open conjugation on Reverso")
            onClicked: Qt.openUrlExternally(root.conjugationUrl)
        }
    }

    GridLayout
    {
        Layout.fillWidth: true
        Layout.bottomMargin: AppMetrics.gapTight
        visible: root.tenses.length > 1
        columns: root.tenseColumns
        rowSpacing: AppMetrics.gapSmall
        columnSpacing: AppMetrics.gapSmall

        Accessible.role: Accessible.PageTabList
        Accessible.name: qsTr("Tenses")

        Repeater
        {
            model: root.tenses

            delegate: DesktopChip
            {
                required property var modelData

                objectName: "dictionary_tense_" + modelData.key

                Layout.row: modelData.row
                Layout.column: modelData.column
                Layout.fillWidth: true
                Layout.preferredWidth: 1

                text: modelData.label
                dotColour: modelData.matched ? AppTheme.colourAccent : "transparent"
                Accessible.description: modelData.matched ? qsTr("Contains the searched form") : ""
                selected: !!root.shownTense && root.shownTense.key === modelData.key

                onClicked: root.chosenKey = modelData.key
            }
        }
    }

    DesktopFormsGrid
    {
        Layout.fillWidth: true
        forms: root.rows
    }

    Flow
    {
        Layout.fillWidth: true
        Layout.topMargin: AppMetrics.gapTight
        visible: root.participles.length > 0
        spacing: AppMetrics.gapLarge

        Repeater
        {
            model: root.participles

            delegate: Row
            {
                required property var modelData

                spacing: AppMetrics.gapSmall

                Accessible.role: Accessible.StaticText
                Accessible.name: modelData.label + " " + modelData.text

                DesktopLabel
                {
                    anchors.baseline: participleText.baseline
                    text: modelData.label
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted

                    Accessible.ignored: true
                }

                DesktopLabel
                {
                    id: participleText

                    text: modelData.text
                    color: modelData.matched ? AppTheme.colourAccent : AppTheme.colourText
                    font.underline: modelData.matched
                    font.pixelSize: AppMetrics.dictionaryFontBody
                    font.weight: Font.Medium

                    Accessible.ignored: true
                }
            }
        }
    }
}
