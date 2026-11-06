import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property string title: ""
    property string format: ""
    property string placeIcon: ""
    property color coverColour: AppTheme.colourSunken
    property color inkColour: AppTheme.colourCoverInk
    property bool missing: false
    property bool highlighted: false
    property bool progressShown: false
    property real progress: 0
    property bool progressAnimated: false
    property real titleSize: AppMetrics.fontSubtitle
    property real spineShown: 0

    radius: AppMetrics.coverRadius
    color: root.highlighted && !root.missing ? Qt.tint(root.coverColour, AppTheme.faded(AppTheme.colourText, AppTheme.alphaFaint))
                                             : root.coverColour
    border.width: root.missing ? AppMetrics.borderWidth : 0
    border.color: AppTheme.colourBorder

    Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

    CoverStripe
    {
        visible: !root.missing
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: AppMetrics.gapCompact
        radius: root.radius
        visible: root.spineShown > 0
        opacity: root.spineShown

        gradient: Gradient
        {
            orientation: Gradient.Horizontal

            GradientStop { position: 0.0; color: AppTheme.faded(AppTheme.colourScrim, AppTheme.alphaStrong) }
            GradientStop { position: 0.45; color: AppTheme.faded(AppTheme.colourScrim, AppTheme.alphaFaint) }
            GradientStop { position: 1.0; color: AppTheme.faded(AppTheme.colourScrim, 0) }
        }
    }

    RowLayout
    {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: AppMetrics.gapMedium
        spacing: AppMetrics.gapSmall

        DesktopLabel
        {
            text: root.format.toUpperCase()
            font.pixelSize: AppMetrics.fontSmall
            font.weight: Font.DemiBold
            font.letterSpacing: AppMetrics.borderWidth
            color: root.inkColour
            opacity: AppTheme.opacitySecondary
        }

        DesktopIcon
        {
            visible: !root.missing && root.placeIcon.length > 0
            name: root.placeIcon
            color: root.inkColour
            opacity: AppTheme.opacitySecondary
        }

        DesktopLabel
        {
            visible: root.missing
            text: qsTr("File not found · click to find it")
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourWarn
        }
    }

    DesktopLabel
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: AppMetrics.gapMedium
        anchors.bottomMargin: AppMetrics.gapMedium + AppMetrics.coverProgressHeight

        text: root.title
        font.pixelSize: Math.round(root.titleSize)
        font.weight: Font.DemiBold
        color: root.inkColour
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }

    Rectangle
    {
        id: track

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: AppMetrics.gapMedium
        anchors.bottomMargin: AppMetrics.gapSmall
        height: AppMetrics.coverProgressHeight
        radius: height / 2
        visible: !root.missing && root.progressShown
        color: AppTheme.faded(root.inkColour, AppTheme.alphaStrong)

        Rectangle
        {
            width: track.width * Math.max(0, Math.min(1, root.progress))
            height: parent.height
            radius: parent.radius
            color: root.inkColour

            Behavior on width
            {
                enabled: root.progressAnimated

                NumberAnimation { duration: AppMetrics.motionSlow * 2; easing.type: AppMetrics.motionEasing }
            }
        }
    }
}
