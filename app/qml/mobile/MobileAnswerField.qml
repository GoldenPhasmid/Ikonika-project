import QtQuick
import QtQuick.Layouts

MobileTextField
{
    id: root

    property bool marked: false
    property bool correct: false
    property string heldMiss: ""
    readonly property bool holdingMiss: root.heldMiss.length > 0

    function holdMiss()
    {
        root.heldMiss = root.text
        root.cursorPosition = root.text.length
    }

    onTextEdited:
    {
        if (!root.holdingMiss)
        {
            return
        }

        const typed = root.text.startsWith(root.heldMiss) ? root.text.slice(root.heldMiss.length) : ""

        root.heldMiss = ""
        root.text = typed
    }

    onTextChanged: if (root.text.length === 0) root.heldMiss = ""

    Layout.fillWidth: true
    Layout.preferredHeight: AppMetrics.buttonHeightLarge

    placeholderText: qsTr("Type translation...")

    font.pixelSize: AppMetrics.sessionInputSize
    horizontalAlignment: TextInput.AlignHCenter

    color:
    {
        if (!root.marked)
        {
            return AppTheme.colourText
        }

        return root.correct ? AppTheme.colourCorrect : AppTheme.colourWrong
    }

    transform: MobileShake { id: shakeMotion }

    function shake()
    {
        shakeMotion.play()
    }

    Behavior on color { ColorAnimation { duration: AppMetrics.motionBase } }
}
