import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import "MobileIcons.js" as Icons

Item
{
    id: root

    required property string label
    property int count: 0
    property bool counted: true
    property string unit: ""
    property bool active: false
    property bool compactPhone: false
    property real progress: -1

    required property color tone
    required property color ink
    required property color deep

    signal clicked()

    readonly property real compactHeight: root.compactPhone ? 80 : 92
    readonly property real heroHeight: root.compactPhone ? 172 : 208
    readonly property real countSize: root.compactPhone ? 70 : 88
    readonly property real catHeight: Math.min(root.compactPhone ? 144 : 176, root.heroHeight * 0.86)
    readonly property real cornerRadius: 26
    readonly property bool finished: root.counted && root.count === 0

    property real bandHeight: root.active ? root.heroHeight : root.compactHeight
    property real catRise: root.active ? 0 : 1.1

    Behavior on catRise
    {
        SequentialAnimation
        {
            PauseAnimation { duration: root.active ? 380 : 0 }
            NumberAnimation
            {
                duration: root.active ? 720 : 260
                easing.type: root.active ? Easing.OutBack : Easing.InCubic
            }
        }
    }

    readonly property var wedgeOutline:
    {
        const radius = root.cornerRadius
        const corners = [[root.width - radius, radius, -90], [root.width - radius, root.height - radius, 0],
                         [radius, root.height - radius, 90], [radius, radius, 180]]
        const band = []

        for (const [centreX, centreY, startAngle] of corners)
        {
            for (let step = 0; step <= 12; ++step)
            {
                const angle = (startAngle + step * 7.5) * Math.PI / 180
                band.push(Qt.point(centreX + radius * Math.cos(angle), centreY + radius * Math.sin(angle)))
            }
        }

        const startX = root.width - 200
        const startY = root.height
        const riseX = 200
        const riseY = -118
        const side = Point => riseX * (Point.y - startY) - riseY * (Point.x - startX)
        const outline = []

        for (let index = 0; index < band.length; ++index)
        {
            const from = band[index]
            const to = band[(index + 1) % band.length]
            const fromSide = side(from)
            const toSide = side(to)

            if (fromSide >= 0)
            {
                outline.push(from)
            }

            if ((fromSide >= 0) === (toSide >= 0))
            {
                continue
            }

            const ratio = fromSide / (fromSide - toSide)
            outline.push(Qt.point(from.x + (to.x - from.x) * ratio, from.y + (to.y - from.y) * ratio))
        }

        return outline
    }

    Behavior on bandHeight
    {
        NumberAnimation { duration: 640; easing.type: Easing.OutCubic }
    }

    Layout.fillWidth: true
    Layout.preferredHeight: root.bandHeight

    clip: true
    scale: tap.pressed ? 0.985 : 1

    Behavior on scale
    {
        NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
    }

    Accessible.role: Accessible.Button
    Accessible.name: root.finished ? qsTr("%1, done").arg(root.label)
                                   : root.counted ? qsTr("%1, %2").arg(root.label).arg(root.count) : root.label
    Accessible.onPressAction: root.clicked()

    Rectangle
    {
        anchors.fill: parent
        radius: root.cornerRadius
        color: root.tone
    }

    Shape
    {
        anchors.fill: parent
        opacity: root.active ? 1 : 0
        preferredRendererType: Shape.CurveRenderer

        Behavior on opacity
        {
            NumberAnimation { duration: AppMetrics.motionSlow }
        }

        ShapePath
        {
            strokeWidth: -1

            fillGradient: RadialGradient
            {
                centerX: 0
                centerY: 0
                centerRadius: root.width * 0.75
                focalX: 0
                focalY: 0

                GradientStop { position: 0; color: Qt.rgba(1, 1, 1, 0.22) }
                GradientStop { position: 0.6; color: Qt.rgba(1, 1, 1, 0) }
            }

            PathRectangle
            {
                width: root.width
                height: root.height
                radius: root.cornerRadius
            }
        }
    }

    Shape
    {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            strokeWidth: -1
            fillColor: root.deep

            PathPolyline { path: root.wedgeOutline }
        }
    }

    RowLayout
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.compactHeight
        anchors.leftMargin: 20
        anchors.rightMargin: 24
        spacing: AppMetrics.gapCompact

        opacity: root.active ? 0 : 1

        Behavior on opacity
        {
            NumberAnimation { duration: root.active ? 140 : 260 }
        }

        Text
        {
            Layout.fillWidth: true

            text: root.label
            elide: Text.ElideRight
            font.family: AppMetrics.fontFamily
            font.pixelSize: 22
            font.weight: Font.Bold
            font.letterSpacing: -0.4
            color: root.ink
        }

        Text
        {
            visible: root.counted && !root.finished

            text: root.count
            font.family: AppMetrics.fontFamily
            font.pixelSize: Math.round(root.countSize * 0.5)
            font.weight: Font.Bold
            font.letterSpacing: -1.5
            font.features: { "tnum": 1 }
            color: root.ink
        }

        MobileVectorIcon
        {
            visible: root.finished
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30

            svgPath: Icons.Icons.check.path
            strokeWidth: 3
            color: root.ink
        }
    }

    Item
    {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 18
        anchors.bottomMargin: 18

        opacity: root.active ? 1 : 0

        Behavior on opacity
        {
            SequentialAnimation
            {
                PauseAnimation { duration: root.active ? 220 : 0 }
                NumberAnimation { duration: root.active ? 360 : 160 }
            }
        }

        Text
        {
            anchors.left: parent.left
            anchors.top: parent.top

            text: root.label
            font.family: AppMetrics.fontFamily
            font.pixelSize: 24
            font.weight: Font.Bold
            font.letterSpacing: -0.5
            color: root.ink
        }

        ColumnLayout
        {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: parent.width * 0.54
            spacing: AppMetrics.gapCompact

            RowLayout
            {
                spacing: 10

                Text
                {
                    visible: root.counted && !root.finished
                    Layout.alignment: Qt.AlignBaseline

                    text: root.count
                    font.family: AppMetrics.fontFamily
                    font.pixelSize: root.countSize
                    font.weight: Font.Bold
                    font.letterSpacing: -root.countSize * 0.055
                    font.features: { "tnum": 1 }
                    lineHeight: 0.8
                    color: root.ink
                }

                MobileVectorIcon
                {
                    visible: root.finished
                    Layout.preferredWidth: Math.round(root.countSize * 0.74)
                    Layout.preferredHeight: Math.round(root.countSize * 0.74)
                    Layout.alignment: Qt.AlignBottom

                    svgPath: Icons.Icons.check.path
                    strokeWidth: 3.4
                    color: root.ink
                }

                Text
                {
                    visible: text.length > 0
                    Layout.alignment: root.finished ? Qt.AlignBottom : Qt.AlignBaseline

                    text: root.unit
                    font.family: AppMetrics.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: root.ink
                    opacity: 0.78
                }
            }

            Rectangle
            {
                visible: root.progress >= 0
                Layout.fillWidth: true
                Layout.preferredHeight: 6
                radius: 3
                color: Qt.rgba(root.ink.r, root.ink.g, root.ink.b, 0.2)

                Rectangle
                {
                    width: parent.width * Math.min(Math.max(root.progress, 0), 1)
                    height: parent.height
                    radius: parent.radius
                    color: root.ink
                }
            }
        }
    }

    MouseArea
    {
        id: tap

        anchors.fill: parent

        onClicked: root.clicked()
    }
}
