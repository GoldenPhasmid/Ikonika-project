import QtQuick

Item
{
    id: root

    property string code: ""
    property real radius: Math.min(AppMetrics.radiusSmall, root.height / 6)

    readonly property var flags: ({
        "en": { kind: "us" },
        "uk": { kind: "bands", colours: ["#0057B7", "#FFD700"] },
        "fr": { kind: "bands", vertical: true, colours: ["#002654", "#FFFFFF", "#CE1126"] },
        "it": { kind: "bands", vertical: true, colours: ["#009246", "#FFFFFF", "#CE2B37"] },
        "de": { kind: "bands", colours: ["#000000", "#DD0000", "#FFCE00"] },
        "nl": { kind: "bands", colours: ["#AE1C28", "#FFFFFF", "#21468B"] },
        "pl": { kind: "bands", colours: ["#FFFFFF", "#DC143C"] },
        "es": { kind: "bands", colours: ["#AA151B", "#F1BF00", "#AA151B"], weights: [1, 2, 1] },
        "pt": { kind: "portugal" }
    })

    readonly property var flag: root.flags[root.code] || null

    implicitWidth: AppMetrics.flagWidth
    implicitHeight: AppMetrics.flagWidth * 3 / 4

    Accessible.ignored: true

    function paintBands(context, flag)
    {
        const weights = flag.weights || flag.colours.map(() => 1)
        const total = weights.reduce((Sum, Weight) => Sum + Weight, 0)
        const length = flag.vertical ? width : height
        let offset = 0

        for (let index = 0; index < flag.colours.length; index++)
        {
            const size = length * weights[index] / total

            context.fillStyle = flag.colours[index]

            if (flag.vertical)
            {
                context.fillRect(offset, 0, size + 1, height)
            }
            else
            {
                context.fillRect(0, offset, width, size + 1)
            }

            offset += size
        }
    }

    function paintStar(context, centreX, centreY, outerRadius)
    {
        const innerRadius = outerRadius * 0.382

        context.beginPath()

        for (let point = 0; point < 10; point++)
        {
            const pointRadius = point % 2 === 0 ? outerRadius : innerRadius
            const angle = -Math.PI / 2 + point * Math.PI / 5

            context.lineTo(centreX + pointRadius * Math.cos(angle), centreY + pointRadius * Math.sin(angle))
        }

        context.closePath()
        context.fill()
    }

    function paintUnitedStates(context)
    {
        const stripeHeight = height / 13

        for (let stripe = 0; stripe < 13; stripe++)
        {
            context.fillStyle = stripe % 2 === 0 ? "#B22234" : "#FFFFFF"
            context.fillRect(0, stripe * stripeHeight, width, stripeHeight + 1)
        }

        const cantonWidth = width * 0.57
        const cantonHeight = stripeHeight * 7

        context.fillStyle = "#3C3B6E"
        context.fillRect(0, 0, cantonWidth, cantonHeight)

        context.fillStyle = "#FFFFFF"

        for (let row = 0; row < 9; row++)
        {
            const longRow = row % 2 === 0
            const count = longRow ? 6 : 5

            for (let column = 0; column < count; column++)
            {
                const slot = longRow ? column * 2 + 1 : column * 2 + 2

                root.paintStar(context, cantonWidth * slot / 12, cantonHeight * (row + 1) / 10, height * 0.032)
            }
        }
    }

    function paintPortugal(context)
    {
        root.paintBands(context, { vertical: true, colours: ["#046A38", "#DA291C"], weights: [2, 3] })

        const centreX = width * 0.4
        const centreY = height / 2
        const sphereRadius = height * 0.21

        context.lineWidth = Math.max(1, height * 0.05)
        context.strokeStyle = "#FFE000"
        context.beginPath()
        context.arc(centreX, centreY, sphereRadius, 0, Math.PI * 2)
        context.stroke()

        context.fillStyle = "#FFFFFF"
        context.fillRect(centreX - sphereRadius * 0.45, centreY - sphereRadius * 0.55, sphereRadius * 0.9, sphereRadius * 1.0)
        context.fillStyle = "#DA291C"
        context.fillRect(centreX - sphereRadius * 0.32, centreY - sphereRadius * 0.42, sphereRadius * 0.64, sphereRadius * 0.74)
    }

    Canvas
    {
        id: canvas

        anchors.fill: parent
        visible: root.flag !== null
        antialiasing: true

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onVisibleChanged: if (visible) requestPaint()

        onPaint:
        {
            const context = getContext("2d")

            context.reset()

            if (!root.flag)
            {
                return
            }

            context.beginPath()
            context.roundedRect(0, 0, width, height, root.radius, root.radius)
            context.clip()

            if (root.flag.kind === "us")
            {
                root.paintUnitedStates(context)

                return
            }

            if (root.flag.kind === "portugal")
            {
                root.paintPortugal(context)

                return
            }

            root.paintBands(context, root.flag)
        }

        Connections
        {
            target: root

            function onFlagChanged() { canvas.requestPaint() }
        }
    }

    Rectangle
    {
        anchors.fill: parent
        visible: root.flag === null
        radius: root.radius
        color: AppTheme.colourWell

        Text
        {
            anchors.centerIn: parent
            text: root.code.toUpperCase()
            font.family: AppMetrics.fontFamily
            font.pixelSize: parent.height * 0.4
            font.weight: Font.DemiBold
            color: AppTheme.colourMuted
        }
    }

    Rectangle
    {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
    }
}
