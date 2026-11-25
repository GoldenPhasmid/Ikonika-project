import QtQuick
import QtQuick.Shapes

Item
{
    id: root

    required property string title
    required property string author
    required property var coverStyle
    property real radius: AppMetrics.bookRadius

    readonly property real inset: Math.round(root.width * AppMetrics.bookCoverInset)
    readonly property real textWidth: Math.round(root.width * AppMetrics.bookTextWidth)
    readonly property real bandRise: root.height * AppMetrics.bookBandRise
    readonly property real bandTop: root.height * AppMetrics.bookBandTop
    readonly property real bandBottom: root.bandTop + root.height * AppMetrics.bookBandHeight
    readonly property real foilHeight: Math.max(AppMetrics.borderWidth, root.height * AppMetrics.bookBandLine)
    readonly property real hairlineHeight: root.foilHeight / 2
    readonly property real hairlineStrength: root.height * AppMetrics.bookBandLine < AppMetrics.borderWidth ? 0 : AppTheme.opacitySecondary
    readonly property real hairlineOffset: root.height * AppMetrics.bookBandHairline

    component Slash: ShapePath
    {
        id: slash

        required property real top
        required property real thickness

        strokeWidth: -1
        startX: 0
        startY: slash.top + root.bandRise

        PathLine { x: root.width; y: slash.top }
        PathLine { x: root.width; y: slash.top + slash.thickness }
        PathLine { x: 0; y: slash.top + slash.thickness + root.bandRise }
        PathLine { x: 0; y: slash.top + root.bandRise }
    }

    component Foil: Slash
    {
        id: foil

        property real strength: 1

        fillGradient: LinearGradient
        {
            x1: 0
            y1: foil.top + root.bandRise
            x2: root.width
            y2: foil.top

            GradientStop { position: 0; color: AppTheme.faded(root.coverStyle.foilLight, foil.strength) }
            GradientStop { position: 0.5; color: AppTheme.faded(root.coverStyle.foilDeep, foil.strength) }
            GradientStop { position: 1; color: AppTheme.faded(root.coverStyle.foil, foil.strength) }
        }
    }

    Shape
    {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            strokeWidth: -1
            fillGradient: LinearGradient
            {
                x1: 0
                y1: 0
                x2: root.width
                y2: root.height

                GradientStop { position: 0; color: root.coverStyle.paperLight }
                GradientStop { position: 0.6; color: root.coverStyle.paper }
                GradientStop { position: 1; color: root.coverStyle.paperDeep }
            }

            PathRectangle { width: root.width; height: root.height; radius: root.radius }
        }

        Slash
        {
            top: root.bandTop
            thickness: root.bandBottom - root.bandTop
            fillColor: AppTheme.faded(AppTheme.colourScrim, AppTheme.alphaMedium)
        }

        Foil
        {
            top: root.bandTop - root.hairlineOffset
            thickness: root.hairlineHeight
            strength: root.hairlineStrength
        }

        Foil
        {
            top: root.bandTop
            thickness: root.foilHeight
        }

        Foil
        {
            top: root.bandBottom
            thickness: root.foilHeight
        }

        Foil
        {
            top: root.bandBottom + root.hairlineOffset
            thickness: root.hairlineHeight
            strength: root.hairlineStrength
        }
    }

    Rectangle
    {
        width: Math.round(root.width * AppMetrics.bookSpineWidth)
        height: root.height
        topLeftRadius: root.radius
        bottomLeftRadius: root.radius
        color: AppTheme.faded(AppTheme.colourScrim, AppTheme.alphaMedium)
    }

    Text
    {
        x: root.inset
        y: root.inset
        width: root.textWidth
        height: root.bandTop - root.hairlineOffset
                + root.bandRise * (1 - (root.inset + root.textWidth) / Math.max(1, root.width)) - root.inset
        text: root.title
        textFormat: Text.PlainText
        font.family: AppMetrics.bookFontFamily
        font.weight: Font.DemiBold
        font.pixelSize: Math.max(1, Math.round(root.width * AppMetrics.bookTitleScale))
        minimumPixelSize: Math.max(1, Math.round(root.width * AppMetrics.bookTitleMinimumScale))
        fontSizeMode: Text.Fit
        lineHeight: AppMetrics.bookTitleLineHeight
        color: root.coverStyle.ink
        wrapMode: Text.Wrap
        elide: Text.ElideRight
    }

    Text
    {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.inset
        width: root.textWidth
        text: root.author
        textFormat: Text.PlainText
        horizontalAlignment: Text.AlignRight
        font.family: AppMetrics.bookFontFamily
        font.weight: Font.Medium
        font.italic: true
        font.pixelSize: Math.max(1, Math.round(root.width * AppMetrics.bookAuthorScale))
        color: root.coverStyle.foil
        elide: Text.ElideRight
        maximumLineCount: 1
    }
}
