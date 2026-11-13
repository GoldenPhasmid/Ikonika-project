import QtQuick
import QtQuick.Controls.Basic

Item
{
    id: root

    property int rank: 0
    property int maximumRank: 0
    property var levelMarks: []
    property int tickFontSize: AppMetrics.fontSmall

    signal rankChosen(int rank)

    readonly property int floorRank: 500
    readonly property int shownRank: slider.pressed ? root.rankAt(slider.value) : root.rank
    readonly property string shownLevel: root.levelOf(root.shownRank)
    readonly property string valueText: root.shownRank <= 0 ? qsTr("The top")
        : root.shownLevel.length > 0
            ? qsTr("%1, about %2").arg(Qt.locale().toString(root.shownRank, "f", 0)).arg(root.shownLevel)
            : Qt.locale().toString(root.shownRank, "f", 0)
    readonly property var numberMarks: [1000, 2000, 5000, 10000, 20000, 50000].filter(Mark => Mark < root.maximumRank)
    readonly property real trackLeft: AppMetrics.rankHandleSize / 2
    readonly property real trackWidth: width - AppMetrics.rankHandleSize

    function positionOf(value)
    {
        if (root.maximumRank <= root.floorRank || value < root.floorRank)
        {
            return 0
        }

        return Math.log(Math.min(value, root.maximumRank) / root.floorRank) / Math.log(root.maximumRank / root.floorRank)
    }

    function rankAt(position)
    {
        if (position <= 0 || root.maximumRank <= root.floorRank)
        {
            return 0
        }

        const raw = Math.round(root.floorRank * Math.pow(root.maximumRank / root.floorRank, position))
        const step = Math.pow(10, Math.max(0, Math.floor(Math.log10(raw)) - 1))

        return Math.min(root.maximumRank, Math.round(raw / step) * step)
    }

    function levelOf(value)
    {
        if (value <= 0 || root.levelMarks.length === 0)
        {
            return ""
        }

        let nearest = root.levelMarks[0]

        for (const Mark of root.levelMarks)
        {
            if (Math.abs(Math.log(Mark.rank / value)) < Math.abs(Math.log(nearest.rank / value)))
            {
                nearest = Mark
            }
        }

        return nearest.label
    }

    function shortNumber(value)
    {
        return value >= 1000 ? qsTr("%1k").arg(Math.round(value / 1000)) : String(value)
    }

    implicitHeight: track.height + AppMetrics.gapSmall + numberMetrics.height

    TextMetrics
    {
        id: numberMetrics

        font.family: AppMetrics.fontFamily
        font.pixelSize: root.tickFontSize
        text: qsTr("50k")
    }

    Item
    {
        id: track

        width: parent.width
        height: AppMetrics.rankTrackWidth + AppMetrics.gapSmall * 2

        Rectangle
        {
            x: root.trackLeft
            anchors.verticalCenter: parent.verticalCenter
            width: root.trackWidth
            height: AppMetrics.rankTrackWidth
            radius: AppMetrics.radiusSmall
            color: AppTheme.colourSunken
            border.width: AppMetrics.borderWidth
            border.color: AppTheme.colourBorder

            Rectangle
            {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: root.trackWidth * slider.visualPosition
                radius: AppMetrics.radiusSmall
                color: AppTheme.colourAccent
            }
        }

        Slider
        {
            id: slider

            objectName: "settings_word_range"

            x: root.trackLeft - AppMetrics.rankHandleSize / 2
            width: root.width
            height: parent.height
            orientation: Qt.Horizontal
            from: 0
            to: 1
            value: root.positionOf(root.rank)
            padding: 0
            focusPolicy: Qt.NoFocus

            Accessible.name: qsTr("Words from rank %1").arg(root.shownRank)

            background: Item {}

            handle: Rectangle
            {
                x: slider.visualPosition * (slider.width - width)
                y: 0
                width: AppMetrics.rankHandleSize
                height: slider.height
                radius: width / 2
                color: AppTheme.colourText
            }

            onPressedChanged:
            {
                if (!slider.pressed)
                {
                    root.rankChosen(root.rankAt(slider.value))
                    slider.value = Qt.binding(() => root.positionOf(root.rank))
                }
            }

            HoverHandler
            {
                cursorShape: Qt.PointingHandCursor
            }
        }
    }

    Repeater
    {
        model: root.numberMarks

        delegate: Text
        {
            required property int modelData

            x: Math.max(0, Math.min(root.width - width, root.trackLeft + root.trackWidth * root.positionOf(modelData) - width / 2))
            y: track.height + AppMetrics.gapSmall
            text: root.shortNumber(modelData)
            font.family: AppMetrics.fontFamily
            font.pixelSize: root.tickFontSize
            color: AppTheme.colourMuted
        }
    }
}
