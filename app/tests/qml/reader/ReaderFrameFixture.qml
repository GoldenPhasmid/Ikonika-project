import QtQuick
import QtQuick.Layouts

MobileReaderFrame
{
    width: 400
    height: 700

    topContent: Rectangle
    {
        Layout.fillWidth: true
        implicitHeight: 120
    }

    bottomContent: Rectangle
    {
        objectName: "navigation"
        Layout.fillWidth: true
        implicitHeight: 80
    }

    MobileReaderPdfView
    {
        objectName: "pdf"
        anchors.fill: parent
        source: "fixture.pdf"
        cropMargins: false
    }
}
