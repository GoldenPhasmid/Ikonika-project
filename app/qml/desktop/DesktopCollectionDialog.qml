import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DesktopDialog
{
    id: root

    property string collectionId: ""
    property int colour: 0
    property var action: null
    property string newTitle: qsTr("New collection")
    property string renameTitle: qsTr("Rename collection")
    property bool colourShown: true

    readonly property string name: nameField.text.trim()
    readonly property bool renaming: root.collectionId.length > 0

    function create(action)
    {
        root.collectionId = ""
        root.title = root.newTitle
        root.colour = 0
        root.action = action
        nameField.text = ""
        root.open()
    }

    function rename(collectionId, name, action)
    {
        root.collectionId = collectionId
        root.title = root.renameTitle
        root.action = action
        nameField.text = name
        root.open()
    }

    function confirm()
    {
        if (root.name.length === 0)
        {
            return
        }

        const action = root.action
        const name = root.name
        const colour = root.colour

        root.close()

        if (action)
        {
            action(name, colour)
        }
    }

    onOpened:
    {
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapMedium

        DesktopFormField
        {
            Layout.fillWidth: true
            label: qsTr("Name")

            DesktopTextField
            {
                id: nameField

                objectName: "collection_name"

                Layout.fillWidth: true
                maximumLength: 60

                Keys.onReturnPressed: root.confirm()
                Keys.onEnterPressed: root.confirm()
            }
        }

        DesktopFormField
        {
            Layout.fillWidth: true
            visible: !root.renaming && root.colourShown
            label: qsTr("Colour")

            RowLayout
            {
                spacing: AppMetrics.gapSmall

                Accessible.role: Accessible.RadioButton

                Repeater
                {
                    model: AppTheme.coverColours

                    delegate: AbstractButton
                    {
                        id: swatch

                        required property color modelData
                        required property int index

                        readonly property bool chosen: root.colour === index

                        implicitWidth: AppMetrics.buttonHeight
                        implicitHeight: AppMetrics.buttonHeight
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus

                        Accessible.role: Accessible.RadioButton
                        Accessible.name: qsTr("Colour %1").arg(index + 1)
                        Accessible.checked: swatch.chosen

                        onClicked: root.colour = index

                        contentItem: Item {}

                        background: Rectangle
                        {
                            radius: AppMetrics.radiusSmall
                            color: swatch.modelData
                            border.width: swatch.chosen || swatch.visualFocus ? AppMetrics.focusRingWidth : 0
                            border.color: AppTheme.colourText
                            opacity: swatch.hovered || swatch.chosen ? 1.0 : AppTheme.opacityHover

                            DesktopIcon
                            {
                                anchors.centerIn: parent
                                visible: swatch.chosen
                                name: "check"
                                color: AppTheme.colourCoverInk
                                width: AppMetrics.iconSizeSmall
                                height: AppMetrics.iconSizeSmall
                            }
                        }
                    }
                }
            }
        }
    }

    footer: [
        DesktopButton
        {
            large: true
            text: qsTr("Cancel")

            onClicked: root.close()
        },
        DesktopButton
        {
            objectName: "collection_confirm"

            large: true
            tone: "primary"
            enabled: root.name.length > 0
            text: root.renaming ? qsTr("Rename") : qsTr("Create")

            onClicked: root.confirm()
        }
    ]
}
