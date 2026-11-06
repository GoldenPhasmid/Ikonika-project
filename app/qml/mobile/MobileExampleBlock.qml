import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

ColumnLayout
{
    id: root

    property string sentence: ""
    property string translation: ""
    property bool collapsible: false
    property bool expanded: false

    signal wordTapped(string word, string sentence)

    function wordUnder(source, x, y)
    {
        if (!panel.open)
        {
            return ""
        }

        const point = sentenceText.mapFromItem(source, x, y)

        return sentenceText.wordAt(point.x, point.y)
    }

    onSentenceChanged: root.expanded = false

    visible: root.sentence.length > 0
    Layout.fillWidth: true
    spacing: 0

    FontMetrics
    {
        id: sentenceMetrics

        font: sentenceText.font
    }

    FontMetrics
    {
        id: translationMetrics

        font: translationText.font
    }

    Rectangle
    {
        id: panel

        readonly property bool open: !root.collapsible || root.expanded

        Layout.fillWidth: true
        Layout.preferredHeight: panel.open ? panel.implicitHeight : 0
        implicitHeight: column.implicitHeight + AppMetrics.gapMedium * 2
        opacity: panel.open ? 1 : 0
        clip: true
        radius: AppMetrics.radiusMedium
        color: AppTheme.colourSunken

        Behavior on Layout.preferredHeight
        {
            enabled: root.collapsible

            NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        }

        Behavior on opacity
        {
            enabled: root.collapsible

            NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        }

        ColumnLayout
        {
            id: column

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.gapMedium
            spacing: 0

            MobileSentenceText
            {
                id: sentenceText

                Layout.bottomMargin: AppMetrics.gapCompact
                Layout.preferredHeight: Math.max(implicitHeight, sentenceMetrics.lineSpacing * 2)
                verticalAlignment: Text.AlignVCenter

                sentence: root.sentence
                font.pixelSize: AppMetrics.fontLead
                font.weight: Font.Normal
                holdEnabled: false
                tapEnabled: true

                onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
            }

            MobileSeparator
            {
                visible: root.translation.length > 0
                strong: true
                Layout.leftMargin: AppMetrics.gapLarge
                Layout.rightMargin: AppMetrics.gapLarge
            }

            Text
            {
                id: translationText

                visible: root.translation.length > 0
                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.gapCompact
                Layout.preferredHeight: Math.max(implicitHeight, translationMetrics.lineSpacing * 2)
                verticalAlignment: Text.AlignVCenter

                text: root.translation
                font.pixelSize: AppMetrics.sessionOptionSize
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourMuted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }

    Item
    {
        visible: root.collapsible
        Layout.fillWidth: true
        implicitHeight: AppMetrics.headerChevronSize + AppMetrics.gapSmall

        Accessible.role: Accessible.Button
        Accessible.name: root.expanded ? qsTr("Hide example") : qsTr("Show example")
        Accessible.onPressAction: root.expanded = !root.expanded

        Item
        {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: AppMetrics.headerChevronSize
            height: AppMetrics.headerChevronSize
            rotation: root.expanded ? 180 : 0

            Behavior on rotation { NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing } }

            MobileVectorIcon
            {
                anchors.fill: parent
                svgPath: Icons.Icons.chevron_up.path
                sourceWidth: Icons.Icons.chevron_up.width
                sourceHeight: Icons.Icons.chevron_up.height
                color: AppTheme.colourMuted
            }
        }

        MouseArea
        {
            anchors.fill: parent
            anchors.margins: -AppMetrics.gapCompact

            onClicked:
            {
                root.expanded = !root.expanded
                console.info("Example block", root.expanded ? "expanded" : "collapsed")
            }
        }
    }
}
