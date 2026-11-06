import QtQuick
import QtQuick.Controls

// The wireframes' .btn, in its three forms: default, `filled` for .primary, and `ghost` for the
// borderless variant. Square-cornered and 32px, unlike AccentButton which is the app's existing
// rounded 42px button. Both exist during the v3 rollout; screens built to the wireframes use this.
Button
{
    id: control

    property bool filled: false
    property bool ghost: false

    /** For an action that throws work away. Outlined in the colour a wrong answer uses rather than
        filled with it: the button has to be readable as dangerous without competing with the
        primary action beside it for attention. */
    property bool destructive: false


    focusPolicy: Qt.NoFocus

    implicitHeight: AppMetrics.buttonHeight
    leftPadding: AppMetrics.gapMedium
    rightPadding: AppMetrics.gapMedium

    font.pixelSize: AppMetrics.fontSmall
    font.family: AppMetrics.fontFamily

    opacity: enabled ? 1 : 0.35

    contentItem: Text
    {
        text: control.text
        color: control.destructive
            ? AppTheme.colourWrong
            : (control.filled ? AppTheme.colourPanel : AppTheme.colourText)
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusSmall

        color:
        {
            if (control.filled)
            {
                return control.down ? Qt.darker(AppTheme.colourText, 1.25) : AppTheme.colourText
            }

            if (control.ghost)
            {
                return control.down ? AppTheme.colourBorder : "transparent"
            }

            return control.down ? AppTheme.colourBorder : AppTheme.colourPanel
        }

        border.width: control.ghost || control.filled ? 0 : AppMetrics.borderWidth

        border.color:
        {
            if (control.destructive)
            {
                return AppTheme.colourWrong
            }

            return control.hovered ? AppTheme.colourAccent : AppTheme.colourBorder
        }
    }
}
