import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

ColumnLayout
{
    id: root

    property string title: qsTr("Ikonika")
    property bool showBack: false
    property string leadingIcon: ""
    property string trailingIcon: ""

    property bool showMute: false
    property bool muted: false
    property bool showSlowSpeech: false
    property bool showOptions: false
    property bool showSearch: false
    property bool showSettings: false
    property bool showSearchField: false
    property bool titleTappable: false
    property real contentMaximumWidth: Number.POSITIVE_INFINITY

    property alias status: statusRow.data
    property alias searchFieldText: searchField.text
    property alias searchFieldPreeditText: searchField.preeditText

    signal backRequested()
    signal leadingActivated()
    signal trailingActivated()
    signal muteRequested()
    signal slowSpeechRequested()
    signal optionsRequested()
    signal searchRequested()
    signal settingsRequested()
    signal searchFieldAccepted()
    signal titleTapped()

    function focusSearchField()
    {
        searchField.forceActiveFocus()
    }

    Layout.fillWidth: true
    spacing: 0

    RowLayout
    {
        Layout.fillWidth: true
        Layout.maximumWidth: root.contentMaximumWidth - 2 * AppMetrics.contentPadding
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredHeight: AppMetrics.topBarHeight
        Layout.leftMargin: AppMetrics.contentPadding
        Layout.rightMargin: AppMetrics.contentPadding

        Item
        {
            objectName: "header_back"

            visible: root.showBack || root.leadingIcon.length > 0
            Layout.preferredWidth: visible ? AppMetrics.iconTapSize : 0
            Layout.preferredHeight: AppMetrics.topBarHeight

            Accessible.role: Accessible.Button
            Accessible.name: root.showBack ? qsTr("Back") : qsTr("Menu")

            DesktopIcon
            {
                anchors.centerIn: parent
                name: root.showBack ? "arrow_left" : root.leadingIcon
                width: AppMetrics.iconSize
                height: width
                color: AppTheme.colourText
            }

            TapHandler { onTapped: root.showBack ? root.backRequested() : root.leadingActivated() }
        }

        Text
        {
            text: root.title
            font.pixelSize: AppMetrics.fontTitle
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            visible: !root.showSearchField
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            elide: Text.ElideRight

            TapHandler
            {
                enabled: root.titleTappable

                onTapped: root.titleTapped()
            }
        }

        RowLayout
        {
            id: statusRow

            spacing: AppMetrics.gapSmall
            visible: children.length > 0
        }

        MobileHeaderSearchField
        {
            id: searchField

            visible: root.showSearchField
            Layout.fillWidth: true

            onAccepted: root.searchFieldAccepted()
        }

        Item
        {
            objectName: "header_slow_speech"

            visible: root.showSlowSpeech
            Layout.preferredWidth: visible ? AppMetrics.iconTapSize : 0
            Layout.preferredHeight: AppMetrics.topBarHeight

            MobileVectorIcon
            {
                anchors.centerIn: parent
                width: AppMetrics.iconSize
                height: AppMetrics.iconSize
                svgPath: Icons.Icons.turtle.path
                color: AppTheme.colourText
            }

            TapHandler { onTapped: root.slowSpeechRequested() }
        }

        Item
        {
            visible: root.showMute
            Layout.preferredWidth: visible ? AppMetrics.iconTapSize : 0
            Layout.preferredHeight: AppMetrics.topBarHeight

            MobileVectorIcon
            {
                anchors.centerIn: parent
                width: AppMetrics.iconSize
                height: AppMetrics.iconSize
                svgPath: root.muted ? Icons.Icons.speaker_off.path : Icons.Icons.speaker_on.path
                color: AppTheme.colourText
            }

            TapHandler { onTapped: root.muteRequested() }
        }

        Item
        {
            visible: root.showSearch
            Layout.preferredWidth: visible ? AppMetrics.iconTapSize : 0
            Layout.preferredHeight: AppMetrics.topBarHeight

            MobileVectorIcon
            {
                anchors.centerIn: parent
                width: AppMetrics.iconSize
                height: AppMetrics.iconSize
                svgPath: Icons.Icons.search.path
                color: AppTheme.colourText
            }

            TapHandler { onTapped: root.searchRequested() }
        }

        Item
        {
            objectName: "header_settings"

            visible: root.showSettings
            Layout.preferredWidth: visible ? AppMetrics.iconTapSize : 0
            Layout.preferredHeight: AppMetrics.topBarHeight

            DesktopIcon
            {
                anchors.centerIn: parent
                name: "nav_settings"
                width: AppMetrics.iconSize
                height: width
                color: AppTheme.colourText

                Rectangle
                {
                    visible: AppSyncController.needsReconnect
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: -AppMetrics.gapTight
                    width: AppMetrics.gapSmall
                    height: AppMetrics.gapSmall
                    radius: width / 2
                    color: AppTheme.colourWrong
                }
            }

            TapHandler { onTapped: root.settingsRequested() }
        }

        Item
        {
            visible: root.trailingIcon.length > 0
            Layout.preferredWidth: visible ? AppMetrics.iconTapSize : 0
            Layout.preferredHeight: AppMetrics.topBarHeight

            MobileVectorIcon
            {
                anchors.centerIn: parent
                width: AppMetrics.iconSize
                height: AppMetrics.iconSize
                svgPath: Icons.Icons[root.trailingIcon] ? Icons.Icons[root.trailingIcon].path
                                                          : Icons.Icons.kebab.path
                color: AppTheme.colourText
            }

            TapHandler { onTapped: root.trailingActivated() }
        }

        Item
        {
            visible: root.showOptions
            Layout.preferredWidth: visible ? AppMetrics.iconTapSize : 0
            Layout.preferredHeight: AppMetrics.topBarHeight

            MobileVectorIcon
            {
                anchors.centerIn: parent
                width: AppMetrics.iconSize
                height: AppMetrics.iconSize
                svgPath: Icons.Icons.kebab.path
                color: AppTheme.colourText
            }

            TapHandler { onTapped: root.optionsRequested() }
        }
    }

    Rectangle
    {
        Layout.fillWidth: true
        Layout.preferredHeight: AppMetrics.borderWidth
        color: AppTheme.colourBorder
    }
}
