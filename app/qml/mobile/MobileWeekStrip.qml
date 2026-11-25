import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes

Item
{
    id: root

    required property var days
    property real todayReview: 0
    property real todayLearn: 0
    property bool showTodayReview: true
    property bool showTodayLearn: true
    property color reviewColour: AppTheme.colourAccent
    property color learnColour: AppTheme.colourCorrect

    signal dayRequested(int daysFromToday)

    readonly property real cellWidth: 48
    readonly property real todayWidth: 88
    readonly property real markSize: 30
    readonly property real todayMarkSize: 68
    readonly property real fadeWidth: 40
    readonly property int busiest: Math.max(1, ...root.days.filter(Day => Day.kind === "coming").map(Day => Day.count))

    Layout.fillWidth: true
    implicitHeight: 14 + 6 + root.todayMarkSize

    function centreToday()
    {
        const todayIndex = root.days.findIndex(Day => Day.kind === "today")

        if (todayIndex < 0)
        {
            return
        }

        const todayCentre = todayIndex * root.cellWidth + root.todayWidth / 2
        strip.contentX = Math.max(0, Math.min(strip.contentWidth - strip.width, todayCentre - strip.width / 2))
    }

    onWidthChanged: root.centreToday()
    onDaysChanged: Qt.callLater(root.centreToday)

    component DashedCircle: Shape
    {
        id: dashed

        property color colour: AppTheme.colourBorder

        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            strokeColor: dashed.colour
            strokeWidth: 1.5
            strokeStyle: ShapePath.DashLine
            dashPattern: [2.5, 2]
            fillColor: "transparent"

            PathAngleArc
            {
                centerX: dashed.width / 2
                centerY: dashed.height / 2
                radiusX: dashed.width / 2 - 0.75
                radiusY: dashed.height / 2 - 0.75
                startAngle: 0
                sweepAngle: 360
            }
        }
    }

    Flickable
    {
        id: strip

        anchors.fill: parent
        contentWidth: row.width
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Row
        {
            id: row

            Repeater
            {
                model: root.days

                delegate: Item
                {
                    id: cell

                    required property var modelData

                    readonly property bool today: cell.modelData.kind === "today"
                    readonly property bool coming: cell.modelData.kind === "coming"

                    width: cell.today ? root.todayWidth : root.cellWidth
                    height: root.height

                    Text
                    {
                        anchors.horizontalCenter: parent.horizontalCenter
                        height: 14

                        text: cell.modelData.name
                        font.family: AppMetrics.fontFamily
                        font.pixelSize: AppMetrics.fontMeta
                        font.weight: cell.today ? Font.Bold : Font.Medium
                        color: cell.today ? AppTheme.colourText : AppTheme.colourMuted
                    }

                    Item
                    {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 20
                        width: cell.today ? root.todayMarkSize : root.markSize
                        height: width

                        MobileDoubleRing
                        {
                            anchors.fill: parent
                            visible: cell.today || (!cell.coming && cell.modelData.recorded)

                            stroke: cell.today ? 6 : 3
                            outerProgress: cell.today ? root.todayReview : (cell.modelData.review ?? 0)
                            innerProgress: cell.today ? root.todayLearn : (cell.modelData.learn ?? 0)
                            showOuter: cell.today ? root.showTodayReview : true
                            showInner: cell.today ? root.showTodayLearn : true
                            outerColour: root.reviewColour
                            innerColour: root.learnColour
                        }

                        DashedCircle
                        {
                            anchors.fill: parent
                            visible: (!cell.today && !cell.coming && !cell.modelData.recorded) || (cell.coming && cell.modelData.count === 0)
                        }

                        Rectangle
                        {
                            anchors.fill: parent
                            visible: cell.coming && cell.modelData.count > 0
                            radius: width / 2
                            color: Qt.tint(AppTheme.colourPanel, Qt.rgba(root.reviewColour.r, root.reviewColour.g, root.reviewColour.b,
                                                                       0.06 + cell.modelData.count / root.busiest * 0.34))
                            border.width: 1.5
                            border.color: Qt.rgba(root.reviewColour.r, root.reviewColour.g, root.reviewColour.b, 0.75)
                        }

                        Text
                        {
                            anchors.centerIn: parent
                            visible: cell.coming

                            text: cell.coming ? cell.modelData.count : ""
                            font.family: AppMetrics.fontFamily
                            font.pixelSize: 11
                            font.weight: cell.coming && cell.modelData.count > 0 ? Font.Bold : Font.Medium
                            color: cell.coming && cell.modelData.count > 0 ? AppTheme.colourText : AppTheme.colourMuted
                        }

                        TapHandler
                        {
                            enabled: cell.coming && cell.modelData.count > 0

                            onTapped: root.dayRequested(cell.modelData.daysFromToday)
                        }
                    }
                }
            }
        }
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.fadeWidth

        gradient: Gradient
        {
            orientation: Gradient.Horizontal

            GradientStop { position: 0; color: AppTheme.colourPanel }
            GradientStop { position: 1; color: Qt.rgba(AppTheme.colourPanel.r, AppTheme.colourPanel.g, AppTheme.colourPanel.b, 0) }
        }
    }

    Rectangle
    {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.fadeWidth

        gradient: Gradient
        {
            orientation: Gradient.Horizontal

            GradientStop { position: 0; color: Qt.rgba(AppTheme.colourPanel.r, AppTheme.colourPanel.g, AppTheme.colourPanel.b, 0) }
            GradientStop { position: 1; color: AppTheme.colourPanel }
        }
    }
}
