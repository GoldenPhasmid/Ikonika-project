import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    heading: qsTr("Tense")

    property var tenses: []

    signal chosen(string tense)

    function describe(tense)
    {
        if (!tense.enabled && tense.fromTriage)
        {
            return qsTr("%1 of %2 verbs studied").arg(tense.count).arg(tense.wanted)
        }

        if (!tense.enabled)
        {
            return tense.count === 0 ? qsTr("Train the present tense of a verb first")
                                     : qsTr("%1 of %2 verbs ready").arg(tense.count).arg(tense.wanted)
        }

        if (tense.fromTriage)
        {
            return qsTr("New verbs")
        }

        return tense.count === 1 ? qsTr("1 verb ready") : qsTr("%1 verbs ready").arg(tense.count)
    }

    MobileOptionListPanel
    {
        Repeater
        {
            model: root.tenses

            delegate: MobileSettingsRow
            {
                id: tenseRow

                required property var modelData
                required property int index

                title: tenseRow.modelData.label
                description: root.describe(tenseRow.modelData)
                lastRow: tenseRow.index === root.tenses.length - 1
                enabled: tenseRow.modelData.enabled
                opacity: tenseRow.modelData.enabled ? 1.0 : 0.4

                onClicked: root.chosen(tenseRow.modelData.key)
            }
        }
    }
}
