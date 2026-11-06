import QtQuick
import QtQuick.Layouts

DesktopLabel
{
    id: root

    property int maximumSize: AppMetrics.sessionWordSize
    property bool speakable: false
    property int gender: -1
    property string verbGroup: ""

    signal speakRequested()

    function escaped(value)
    {
        return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    function withArticle(prompt, article)
    {
        if (article.length === 0 || !prompt.startsWith(article))
        {
            return root.escaped(prompt)
        }

        return "<font color=\"" + AppTheme.colourMuted + "\">" + root.escaped(article) + "</font>"
            + root.escaped(prompt.substring(article.length))
    }

    function speak()
    {
        wiggle.restart()
        root.speakRequested()
    }

    Layout.fillWidth: true
    Layout.fillHeight: true

    color: AppTheme.verbGroupColours[root.verbGroup] ?? AppTheme.genderColours[root.gender] ?? AppTheme.colourText
    font.pixelSize: root.maximumSize
    font.weight: Font.Medium
    fontSizeMode: Text.Fit
    minimumPixelSize: AppMetrics.fontLabel
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignBottom
    wrapMode: Text.WordWrap
    elide: Text.ElideRight
    transformOrigin: Item.Bottom

    Accessible.onPressAction: if (root.speakable) root.speak()

    DesktopWiggleAnimation
    {
        id: wiggle

        target: root
    }

    MouseArea
    {
        objectName: "exercise_prompt_speak"

        visible: root.speakable
        x: (root.width - width) / 2
        y: root.height - height
        width: Math.min(root.contentWidth, root.width)
        height: Math.min(root.contentHeight, root.height)
        cursorShape: Qt.PointingHandCursor

        onClicked: root.speak()
    }
}
