import QtQuick

Item
{
    id: root

    property int page: 0
    property int pageCount: 0
    property bool editable: true
    property string text: (root.page + 1) + " / " + Math.max(root.pageCount, 1)
    property bool editing: false

    signal pageRequested(int page)

    implicitWidth: 180
    implicitHeight: 43

    Accessible.role: Accessible.Button
    Accessible.name: root.editable ? qsTr("Go to page. Current page %1 of %2").arg(root.page + 1).arg(root.pageCount)
                                  : root.text
    Accessible.onPressAction: root.beginEdit()

    onVisibleChanged: if (!visible) root.cancelEdit()

    function beginEdit()
    {
        if (!root.editable || root.pageCount <= 0)
        {
            return
        }

        field.text = String(root.page + 1)
        root.editing = true
        field.forceActiveFocus()
        field.selectAll()
        Qt.inputMethod.show()
    }

    function cancelEdit()
    {
        if (!root.editing)
        {
            return
        }

        root.editing = false
        field.focus = false
        Qt.inputMethod.hide()
    }

    function submitPage()
    {
        const number = Number(field.text.trim())

        if (!Number.isInteger(number) || number < 1 || number > root.pageCount)
        {
            return
        }

        root.pageRequested(number - 1)
        root.cancelEdit()
    }

    Text
    {
        anchors.centerIn: parent
        visible: !root.editing
        text: root.text
        color: AppTheme.colourText
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: root.editable && !root.editing && root.pageCount > 0

        onClicked: root.beginEdit()
    }

    MobileTextField
    {
        id: field

        objectName: "reader_page_number_input"
        anchors.fill: parent
        visible: root.editing
        horizontalAlignment: TextInput.AlignHCenter
        inputMethodHints: Qt.ImhDigitsOnly
        selectByMouse: true

        validator: IntValidator
        {
            bottom: 1
            top: Math.max(1, root.pageCount)
        }

        onAccepted: root.submitPage()
        onActiveFocusChanged: if (!activeFocus) root.cancelEdit()
        Keys.onEscapePressed: root.cancelEdit()
    }
}
