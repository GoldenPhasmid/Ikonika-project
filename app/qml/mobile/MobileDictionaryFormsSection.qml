import QtQuick
import QtQuick.Layouts
import "../DictionaryTenseNames.js" as TenseNames

ColumnLayout
{
    id: root

    required property var forms
    property string headword: ""
    property bool headwordIsFeminine: false

    readonly property var moodNames: root.forms.language === "fr" ? ["", "indicatif", "subjonctif", "conditionnel", "impératif"] : ["", qsTr("Indicative"), qsTr("Subjunctive"),
        qsTr("Conditional"), qsTr("Imperative")]
    readonly property var nonfiniteNames: ["", "", "", "", "", qsTr("Infinitive"), qsTr("Gerund"),
        qsTr("Present participle"), qsTr("Past participle")]
    readonly property var personNames: root.forms.cues || []
    readonly property var genderNames: [qsTr("Masculine"), qsTr("Feminine")]
    readonly property var numberNames: [qsTr("Singular"), qsTr("Plural")]

    property string picker: ""
    property int categoryIndex: 0
    property int optionIndex: 0
    property int pendingCategory: 0

    readonly property bool french: forms.language === "fr"
    readonly property var categories:
    {
        if (forms.kind !== "verb")
        {
            return []
        }

        const groups = forms.moods.map(mood => ({
            label: moodNames[mood.mood],
            options: mood.tenses.map(tense => ({
                label: TenseNames.name(forms.language, mood.mood, tense.tense),
                key: mood.mood + "-" + tense.tense,
                rows: tense.cells.map((form, index) => ({
                    person: personNames[index] || "",
                    form: form,
                    matched: tense.matches[index]
                })).filter(row => row.form.length > 0)
            }))
        }))

        if (forms.nonfinite.length > 0)
        {
            const frenchNames = ["", "", "", "", "", "infinitif", "gérondif",
                "participe présent", "participe passé"]
            groups.push({
                label: french ? "formes non personnelles" : qsTr("Nonfinite forms"),
                options: forms.nonfinite.map(form => ({
                    label: (french ? frenchNames : nonfiniteNames)[form.tense],
                    key: "",
                    rows: [{ person: "", form: form.text, matched: form.matched }]
                }))
            })
        }

        return groups
    }
    readonly property var category: categories[categoryIndex]
    readonly property var option: category ? category.options[optionIndex] : null

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall
    visible: forms.kind !== "none"

    function resetSelection()
    {
        for (let index = 0; index < categories.length; ++index)
        {
            const found = categories[index].options.findIndex(option => option.key !== ""
                && option.key === forms.matchedKey)
            if (found >= 0)
            {
                categoryIndex = index
                optionIndex = found
                return
            }
        }

        categoryIndex = 0
        optionIndex = 0
    }

    function choose(category, option)
    {
        categoryIndex = category
        optionIndex = option
        picker = ""
    }

    onCategoriesChanged: resetSelection()

    MobileDictionaryFormsLabel
    {
        visible: !!root.category
        text: root.category ? root.category.label : ""
    }

    MobileSessionButton
    {
        visible: !!root.option
        label: root.option ? root.option.label : ""
        trailingArrow: "down"
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Choose tense, %1").arg(label)
        Accessible.onPressAction: root.picker = "category"

        onClicked: root.picker = "category"
    }

    Rectangle
    {
        Layout.fillWidth: true
        visible: !!root.option
        implicitHeight: tableRows.implicitHeight + AppMetrics.gapTight * 2
        radius: AppMetrics.radiusMedium
        color: AppTheme.colourPanel
        border.color: AppTheme.colourBorder
        border.width: AppMetrics.borderWidth

        ColumnLayout
        {
            id: tableRows

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: AppMetrics.contentPadding
            anchors.rightMargin: AppMetrics.contentPadding
            anchors.topMargin: AppMetrics.gapTight
            spacing: 0


            Repeater
            {
                model: root.option ? root.option.rows : []

                delegate: ColumnLayout
                {
                    id: formRow

                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    spacing: 0

                    Rectangle
                    {
                        Layout.fillWidth: true
                        visible: formRow.index > 0
                        Layout.preferredHeight: AppMetrics.borderWidth
                        color: AppTheme.colourBorder
                    }

                    Item
                    {
                        Layout.fillWidth: true
                        implicitHeight: Math.max(AppMetrics.buttonHeightLarge,
                            Math.max(person.implicitHeight, spelling.implicitHeight) + AppMetrics.gapMedium * 2)

                        Text
                        {
                            id: person

                            width: parent.width * 0.3 - AppMetrics.gapSmall
                            y: (parent.height - implicitHeight) / 2
                            text: formRow.modelData.person
                            wrapMode: Text.Wrap
                            font.family: AppMetrics.fontFamily
                            font.pixelSize: AppMetrics.fontSmall
                            color: AppTheme.colourMuted
                        }

                        Text
                        {
                            id: spelling

                            x: person.text.length > 0 ? parent.width * 0.3 : 0
                            width: parent.width - x
                            y: (parent.height - implicitHeight) / 2
                            text: formRow.modelData.form
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            font.family: AppMetrics.fontFamily
                            font.pixelSize: AppMetrics.fontTitle
                            font.weight: formRow.modelData.matched ? Font.Bold : Font.Medium
                            color: AppTheme.colourText
                        }
                    }
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
