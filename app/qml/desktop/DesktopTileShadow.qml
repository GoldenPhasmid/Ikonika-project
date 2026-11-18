import QtQuick
import QtQuick.Effects

RectangularShadow
{
    id: root

    property color plateColour: "transparent"

    anchors.fill: parent
    z: -1
    offset: Qt.vector2d(AppMetrics.tileShadowOffsetX, AppMetrics.tileShadowOffsetY)
    blur: AppMetrics.tileShadowBlur
    color: AppTheme.colourTileShadow
    visible: root.color.a > 0 && root.plateColour.a > 0
}
