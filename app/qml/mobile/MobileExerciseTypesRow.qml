import QtQuick
import "MobileIcons.js" as Icons

MobileChipSelectRow
{
    id: root

    property var session: null

    title: qsTr("Allowed exercises")
    blockedHint: qsTr("At least one exercise must stay on")

    options:
    [
        {
            key: "audio",
            label: qsTr("Audio"),
            icon: Icons.Icons.speaker_on.path,
            enabled: root.session ? root.session.audioExercise : true
        },
        {
            key: "word",
            label: qsTr("Type word"),
            icon: Icons.Icons.edit.path,
            enabled: root.session ? root.session.typeWord : true
        },
        {
            key: "gap",
            label: qsTr("Fill the gap"),
            icon: Icons.Icons.edit.path,
            enabled: root.session ? root.session.gapExercise : true
        }
    ]

    onToggled: function(option)
    {
        if (!root.session)
        {
            return
        }

        switch (option.key)
        {
        case "audio":
            root.session.audioExercise = !option.enabled
            break
        case "word":
            root.session.typeWord = !option.enabled
            break
        case "gap":
            root.session.gapExercise = !option.enabled
            break
        }
    }
}
