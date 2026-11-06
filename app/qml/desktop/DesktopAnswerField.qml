import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

TextField
{
    id: root

    property bool marked: false
    property bool correct: false
    property real shakeOffset: 0
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

    function shake()
    {
        shakeMotion.restart()
    }

    Layout.fillWidth: true
    implicitHeight: AppMetrics.buttonHeightLarge + AppMetrics.gapSmall

    leftPadding: AppMetrics.gapMedium
    rightPadding: AppMetrics.gapMedium
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    horizontalAlignment: TextInput.AlignHCenter

    font.family: AppMetrics.fontFamily
    font.pixelSize: AppMetrics.sessionInputSize
    color: !root.marked ? AppTheme.colourText : (root.correct ? AppTheme.colourCorrect : AppTheme.colourWrong)
    placeholderTextColor: AppTheme.colourMuted
    selectionColor: AppTheme.colourAccent
    selectedTextColor: AppTheme.colourAccentText
    selectByMouse: true
    inputMethodHints: Qt.ImhNoPredictiveText

    Accessible.name: root.placeholderText

    transform: Translate { x: root.shakeOffset }

    Behavior on color { ColorAnimation { duration: AppMetrics.motionBase } }

    DesktopShakeAnimation
    {
        id: shakeMotion

        target: root
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.marked
            ? Qt.tint(AppTheme.colourPanel, AppTheme.faded(root.color, AppTheme.alphaSoft))
            : AppTheme.colourPanel
        border.width: root.activeFocus || root.marked ? AppMetrics.focusRingWidth : AppMetrics.borderWidth
        border.color: root.marked ? root.color : (root.activeFocus ? AppTheme.colourAccent : AppTheme.colourBorder)

        Behavior on border.color { ColorAnimation { duration: AppMetrics.motionBase } }
    }
}
