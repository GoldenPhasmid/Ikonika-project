import QtQuick
import QtQuick.Shapes
import "../mobile/MobileIcons.js" as Icons

MobileVectorIcon
{
    id: root

    property string name: ""

    readonly property var glyph: Icons.Icons[root.name] || null

    preferredRendererType: Shape.CurveRenderer

    implicitWidth: AppMetrics.iconSize
    implicitHeight: AppMetrics.iconSize
    width: implicitWidth
    height: implicitHeight

    svgPath: root.glyph ? root.glyph.path : ""
    minX: root.glyph ? root.glyph.minX : 0
    minY: root.glyph ? root.glyph.minY : 0
    sourceWidth: root.glyph ? root.glyph.width : 24
    sourceHeight: root.glyph ? root.glyph.height : 24
}
