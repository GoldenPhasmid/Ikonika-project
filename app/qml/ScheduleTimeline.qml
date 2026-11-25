import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes

ColumnLayout
{
    id: root

    property var paths: []
    property int titleSize: AppMetrics.fontBody
    property bool showsForgotten: true

    readonly property real plotHeight: AppMetrics.gapLarge * 2 + AppMetrics.gapSmall
    readonly property real stroke: AppMetrics.borderWidth * 1.5
    readonly property real barGap: AppMetrics.gapTight
    readonly property real stripe: AppMetrics.gapTight

    readonly property int longestWait: Math.max(1, ...root.paths.map(Path => Math.max(0, ...Path.reviews.map(
        (Review, Index) => Review.day - (Index > 0 ? Path.reviews[Index - 1].day : 0)))))

    function barHeight(wait)
    {
        return Math.max(root.barGap, Math.sqrt(wait / root.longestWait) * root.plotHeight)
    }

    spacing: AppMetrics.gapLarge

    Accessible.role: Accessible.StaticText
    Accessible.name: root.paths.map(Path => Path.reviews.map(Review => Review.day).join(", ")).join("; ")

    Text
    {
        Layout.fillWidth: true
        text: qsTr("Learning path, in days")
        font.family: AppMetrics.fontFamily
        font.pixelSize: root.titleSize
        font.weight: Font.Medium
        color: AppTheme.colourText
    }

    Repeater
    {
        model: root.paths

        Item
        {
            id: chart

            required property var modelData

            readonly property var reviews: chart.modelData.reviews
            readonly property real slot: chart.width / Math.max(1, chart.reviews.length)
            readonly property real baselineY: root.plotHeight + AppMetrics.gapLarge

            Layout.fillWidth: true
            implicitHeight: chart.baselineY + AppMetrics.gapSmall + dayMetrics.height

            TextMetrics
            {
                id: dayMetrics

                font.family: AppMetrics.fontFamily
                font.pixelSize: AppMetrics.fontMicro
                text: "000"
            }

            Rectangle
            {
                y: chart.baselineY
                width: chart.width
                height: AppMetrics.borderWidth
                color: AppTheme.colourBorder
            }

            Repeater
            {
                model: chart.reviews

                Item
                {
                    id: review

                    required property var modelData
                    required property int index

                    readonly property int wait: review.modelData.day - (review.index > 0 ? chart.reviews[review.index - 1].day : 0)
                    readonly property bool forgotten: review.modelData.forgotten
                    readonly property bool learned: review.index === chart.reviews.length - 1 && chart.modelData.completedDay > 0
                    readonly property bool quick: review.modelData.quick && !review.forgotten && !review.learned
                    readonly property real barHeight: root.barHeight(review.wait)

                    x: review.index * chart.slot
                    width: chart.slot
                    height: chart.height

                    Rectangle
                    {
                        visible: !review.forgotten
                        x: root.barGap / 2
                        y: chart.baselineY - review.barHeight
                        width: review.width - root.barGap
                        height: review.barHeight
                        topLeftRadius: AppMetrics.radiusSmall / 2
                        topRightRadius: AppMetrics.radiusSmall / 2
                        color: review.learned ? AppTheme.faded(AppTheme.colourCorrect, AppTheme.alphaStrong)
                             : review.quick ? AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaSoft)
                             : AppTheme.colourAccent
                        border.width: review.learned || review.quick ? root.stroke : 0
                        border.color: review.learned ? AppTheme.colourCorrect : AppTheme.colourAccent
                    }

                    Shape
                    {
                        visible: review.forgotten
                        x: root.barGap / 2
                        y: chart.baselineY - review.barHeight
                        width: review.width - root.barGap
                        height: review.barHeight
                        preferredRendererType: Shape.CurveRenderer

                        ShapePath
                        {
                            strokeWidth: -1
                            fillGradient: LinearGradient
                            {
                                spread: ShapeGradient.RepeatSpread
                                x1: 0
                                y1: 0
                                x2: root.stripe
                                y2: root.stripe

                                GradientStop { position: 0.0; color: AppTheme.colourWrong }
                                GradientStop { position: 0.5; color: AppTheme.colourWrong }
                                GradientStop { position: 0.5001; color: AppTheme.faded(AppTheme.colourWrong, AppTheme.alphaStrong) }
                                GradientStop { position: 1.0; color: AppTheme.faded(AppTheme.colourWrong, AppTheme.alphaStrong) }
                            }

                            PathRectangle
                            {
                                width: review.width - root.barGap
                                height: review.barHeight
                                topLeftRadius: AppMetrics.radiusSmall / 2
                                topRightRadius: AppMetrics.radiusSmall / 2
                            }
                        }
                    }

                    Rectangle
                    {
                        visible: review.forgotten
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: chart.baselineY - review.barHeight - AppMetrics.gapSmall - height
                        width: AppMetrics.gapCompact + AppMetrics.borderWidth * 2
                        height: width
                        radius: width / 2
                        color: AppTheme.colourWrong

                        DesktopIcon
                        {
                            anchors.centerIn: parent
                            width: parent.width - AppMetrics.gapTight
                            height: width
                            name: "close"
                            color: AppTheme.colourPanel
                        }
                    }

                    Text
                    {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: chart.baselineY + AppMetrics.gapSmall
                        text: review.modelData.day
                        font.family: AppMetrics.fontFamily
                        font.pixelSize: AppMetrics.fontMicro
                        font.weight: review.forgotten || review.learned ? Font.DemiBold : Font.Medium
                        color: review.forgotten || review.learned ? AppTheme.colourText : AppTheme.colourMuted
                    }
                }
            }
        }
    }

    Flow
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapMedium

        Repeater
        {
            model: [
                { label: qsTr("Review"), kind: "review" },
                { label: qsTr("Quick review"), kind: "quick" },
                { label: qsTr("Forgot it"), kind: "forgotten" },
                { label: qsTr("Learned"), kind: "learned" }
            ].filter(Entry => root.showsForgotten || Entry.kind !== "forgotten")

            Row
            {
                required property var modelData

                spacing: AppMetrics.gapSmall - AppMetrics.borderWidth * 2

                Item
                {
                    anchors.verticalCenter: parent.verticalCenter
                    width: AppMetrics.gapSmall + AppMetrics.borderWidth * 2
                    height: width

                    Rectangle
                    {
                        anchors.fill: parent
                        visible: modelData.kind !== "forgotten"
                        radius: AppMetrics.borderWidth * 2
                        color: modelData.kind === "learned" ? AppTheme.faded(AppTheme.colourCorrect, AppTheme.alphaStrong)
                             : modelData.kind === "quick" ? AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaSoft)
                             : AppTheme.colourAccent
                        border.width: modelData.kind === "review" ? 0 : root.stroke
                        border.color: modelData.kind === "learned" ? AppTheme.colourCorrect : AppTheme.colourAccent
                    }

                    Shape
                    {
                        anchors.fill: parent
                        visible: modelData.kind === "forgotten"
                        preferredRendererType: Shape.CurveRenderer

                        ShapePath
                        {
                            strokeWidth: -1
                            fillGradient: LinearGradient
                            {
                                spread: ShapeGradient.RepeatSpread
                                x2: root.stripe
                                y2: root.stripe

                                GradientStop { position: 0.0; color: AppTheme.colourWrong }
                                GradientStop { position: 0.5; color: AppTheme.colourWrong }
                                GradientStop { position: 0.5001; color: AppTheme.faded(AppTheme.colourWrong, AppTheme.alphaStrong) }
                                GradientStop { position: 1.0; color: AppTheme.faded(AppTheme.colourWrong, AppTheme.alphaStrong) }
                            }

                            PathRectangle
                            {
                                width: AppMetrics.gapSmall + AppMetrics.borderWidth * 2
                                height: width
                                radius: AppMetrics.borderWidth * 2
                            }
                        }
                    }
                }

                Text
                {
                    text: modelData.label
                    font.family: AppMetrics.fontFamily
                    font.pixelSize: AppMetrics.fontMeta
                    color: AppTheme.colourMuted
                }
            }
        }
    }
}
