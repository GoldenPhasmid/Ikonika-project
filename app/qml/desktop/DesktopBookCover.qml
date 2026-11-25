import QtQuick
import QtQuick.Layouts
import "../ReaderShelf.js" as Shelf

Rectangle
{
    id: root

    property string title: ""
    property string author: ""
    property string imageSource: ""
    property bool missing: false
    property bool progressShown: false
    property real progress: 0
    property bool progressAnimated: false

    readonly property var coverStyle: Shelf.tint(root.title, AppTheme.bookCovers)
    readonly property bool imageShown: !root.missing && image.status === Image.Ready

    radius: AppMetrics.bookRadius
    color: root.missing ? AppTheme.colourSunken : root.coverStyle.paper
    border.width: root.missing ? AppMetrics.borderWidth : 0
    border.color: AppTheme.colourBorder
    clip: true

    Image
    {
        id: image

        anchors.fill: parent
        visible: root.imageShown
        source: root.missing ? "" : root.imageSource
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        smooth: true
        mipmap: Qt.platform.os !== "android"
    }

    Loader
    {
        anchors.fill: parent
        active: !root.missing && image.status !== Image.Ready

        sourceComponent: Component
        {
            DefaultBookCover
            {
                title: root.title
                author: root.author
                coverStyle: root.coverStyle
                radius: root.radius
            }
        }
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: AppMetrics.gapCompact
        visible: root.imageShown

        gradient: Gradient
        {
            orientation: Gradient.Horizontal

            GradientStop { position: 0.0; color: AppTheme.faded(AppTheme.colourScrim, AppTheme.alphaStrong) }
            GradientStop { position: 0.45; color: AppTheme.faded(AppTheme.colourScrim, AppTheme.alphaFaint) }
            GradientStop { position: 1.0; color: AppTheme.faded(AppTheme.colourScrim, 0) }
        }

        opacity: AppTheme.alphaHalf
    }

    ColumnLayout
    {
        anchors.centerIn: parent
        width: parent.width - AppMetrics.gapMedium * 2
        visible: root.missing
        spacing: AppMetrics.gapTight

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.title
            horizontalAlignment: Text.AlignHCenter
            font.weight: Font.DemiBold
            color: AppTheme.colourMuted
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: qsTr("File not found")
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourWarn
            wrapMode: Text.WordWrap
        }
    }

    Rectangle
    {
        id: track

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: AppMetrics.coverProgressHeight
        visible: !root.missing && root.progressShown
        color: AppTheme.faded(AppTheme.colourScrim, AppTheme.alphaHalf)

        Rectangle
        {
            width: track.width * Math.max(0, Math.min(1, root.progress))
            height: parent.height
            color: AppTheme.colourAccent

            Behavior on width
            {
                enabled: root.progressAnimated

                NumberAnimation { duration: AppMetrics.motionSlow * 2; easing.type: AppMetrics.motionEasing }
            }
        }
    }
}
