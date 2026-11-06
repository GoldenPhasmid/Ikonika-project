import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DesktopDialog
{
    id: root

    property var tenses: []

    signal chosen(string tense)

    title: qsTr("Tense")

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

        return qsTr("%n verbs ready", "", tense.count)
    }

    DesktopChoiceGrid
    {
        namePrefix: "tense_"
        choices: root.tenses.map(function(tense)
        {
            return {
                key: tense.key,
                label: tense.label,
                caption: root.describe(tense),
                enabled: tense.enabled,
                progress: tense.enabled ? -1 : tense.count / Math.max(1, tense.wanted)
            }
        })

        onActivated: function(key) { root.closeThen(function() { root.chosen(key) }) }
    }

    footer: DesktopButton
    {
        large: true
        tone: "ghost"
        text: qsTr("Cancel")

        onClicked: root.close()
    }
}
