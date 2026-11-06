import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property var book: null
    property bool cloudAvailable: false

    readonly property bool local: root.book !== null && root.book.local
    readonly property bool exists: root.book !== null && root.book.exists
    readonly property bool inCloud: root.book !== null && root.book.cloudId.length > 0
    readonly property bool free: root.book !== null && root.book.free === true
    readonly property bool canDeleteFromCloud: root.inCloud && root.cloudAvailable

    signal downloadRequested(var book)
    signal uploadRequested(var book)
    signal showRequested(var book)
    signal removeRequested(var book)
    signal cloudDeleteRequested(var book)

    heading: root.book !== null ? root.book.title : ""
    visible: false

    onDismissed: root.close()

    function open(book)
    {
        root.book = book
        root.present()
    }

    function close()
    {
        root.hide()
    }

    function choose(handler)
    {
        const book = root.book

        root.close()
        handler(book)
    }

    MobileOptionListPanel
    {
        MobileSettingsRow
        {
            objectName: "reader_sheet_download"

            visible: !root.local && root.cloudAvailable
            title: qsTr("Download")
            lastRow: !root.canDeleteFromCloud

            onClicked: root.choose(root.downloadRequested)
        }

        MobileSettingsRow
        {
            objectName: "reader_sheet_upload"

            visible: root.local && root.exists && !root.inCloud && !root.free && root.cloudAvailable
            title: qsTr("Upload to the cloud")

            onClicked: root.choose(root.uploadRequested)
        }

        MobileSettingsRow
        {
            objectName: "reader_sheet_show"

            visible: root.local && root.exists && Qt.platform.os === "android"
            title: qsTr("Open in another app")

            onClicked: root.choose(root.showRequested)
        }

        MobileSettingsRow
        {
            objectName: "reader_sheet_remove"

            visible: root.local
            title: root.inCloud ? qsTr("Remove from this phone") : qsTr("Remove from library")
            destructive: true
            lastRow: !root.canDeleteFromCloud

            onClicked: root.choose(root.removeRequested)
        }

        MobileSettingsRow
        {
            objectName: "reader_sheet_cloud_delete"

            visible: root.canDeleteFromCloud
            title: qsTr("Delete from the cloud")
            destructive: true
            lastRow: true

            onClicked: root.choose(root.cloudDeleteRequested)
        }
    }
}
