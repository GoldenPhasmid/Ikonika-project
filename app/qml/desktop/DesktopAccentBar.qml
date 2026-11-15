import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Row
{
    id: root

    property string languageCode: ""
    property var field: null

    readonly property var charactersByLanguage: ({
        "fr": ["à", "â", "ç", "é", "è", "ê", "ë", "î", "ï", "ô", "œ", "ù", "û"],
        "es": ["á", "é", "í", "ó", "ú", "ñ", "ü", "¿", "¡"],
        "de": ["ä", "ö", "ü", "ß"],
        "it": ["à", "è", "é", "ì", "ò", "ù"],
        "pt": ["á", "â", "ã", "à", "ç", "é", "ê", "í", "ó", "ô", "õ", "ú"]
    })
    readonly property var characters: root.charactersByLanguage[root.languageCode] ?? []

    Layout.alignment: Qt.AlignHCenter

    visible: root.characters.length > 0
    enabled: !!root.field && !root.field.readOnly
    spacing: AppMetrics.gapCompact

    Repeater
    {
        model: root.characters

        DesktopButton
        {
            required property string modelData

            objectName: "accent_" + modelData
            text: modelData
            implicitHeight: AppMetrics.chipHeight
            implicitWidth: AppMetrics.chipHeight
            focusPolicy: Qt.NoFocus

            Accessible.name: qsTr("Insert %1").arg(modelData)

            onClicked: root.field.typeCharacter(modelData)
        }
    }
}
