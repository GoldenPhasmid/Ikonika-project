import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle
{
    id: root

    required property var store
    property string destination: "today"
    property int dueCount: 0
    property var crumbs: []
    property bool languageShortcut: true
    default property alias actions: actionRow.data

    signal crumbActivated(int index)
    signal destinationSelected(string key)

    function openLanguageSwitch()
    {
        languageSwitch.open()
    }

    function revealCrumbs()
    {
        crumbReveal.restart()
    }

    ParallelAnimation
    {
        id: crumbReveal

        NumberAnimation { target: crumbRow; property: "opacity"; from: 0; to: 1; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: crumbShift; property: "x"; from: AppMetrics.gapSmall; to: 0; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
    }

    readonly property var catalogue: ({
        today: { key: "today", label: qsTr("Today"), icon: "nav_today" },
        words: { key: "words", label: qsTr("Words"), icon: "nav_words" },
        reader: { key: "reader", label: qsTr("Reader"), icon: "nav_reader" },
        decks: { key: "decks", label: qsTr("Decks"), icon: "nav_decks" },
        vocabulary: { key: "vocabulary", label: qsTr("Vocabulary"), icon: "nav_vocabulary" }
    })

    readonly property var order: (AppViewLayouts.revision, AppViewLayouts.navigationOrder())

    readonly property var destinations: root.order
        .filter(Key => Key !== "decks" || DecksController.available)
        .map(Key => root.catalogue[Key])

    property int dragIndex: -1
    property real dragShift: 0
    property bool reordering: false

    readonly property real slotWidth: AppMetrics.navItemHeight + navRow.spacing

    readonly property int dropIndex: root.dragIndex < 0
                                     ? -1
                                     : Math.max(0, Math.min(root.destinations.length - 1,
                                                            root.dragIndex + Math.round(root.dragShift / root.slotWidth)))

    function slotOffset(index)
    {
        if (index === root.dragIndex)
        {
            return root.dragShift
        }

        if (root.dragIndex < index && index <= root.dropIndex)
        {
            return -root.slotWidth
        }

        if (root.dropIndex <= index && index < root.dragIndex)
        {
            return root.slotWidth
        }

        return 0
    }

    function finishDrag()
    {
        const from = root.dragIndex
        const to = root.dropIndex

        root.reordering = false
        root.dragIndex = -1
        root.dragShift = 0

        if (from < 0 || from === to)
        {
            return
        }

        const key = root.destinations[from].key
        const anchorKey = root.destinations[to].key
        const reordered = root.order.filter(Key => Key !== key)
        const anchor = reordered.indexOf(anchorKey)

        reordered.splice(to > from ? anchor + 1 : anchor, 0, key)

        console.info("Moved destination", key, "from", from + 1, "to", to + 1)
        AppViewLayouts.setNavigationOrder(reordered)
    }

    implicitHeight: AppMetrics.topBarHeight
    color: AppTheme.colourPanel

    DesktopDivider
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    Button
    {
        id: brand

        objectName: "nav_account"

        readonly property bool showsShortcut: AppModifierKeys.controlHeld && root.languageShortcut

        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.gapMedium
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: AppMetrics.navItemHeight
        implicitHeight: AppMetrics.navItemHeight

        padding: 0
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus

        text: qsTr("Ikonika, %1").arg(root.store.languagePairLabel)
        Accessible.name: text
        Accessible.description: qsTr("Switch the language you learn")

        ToolTip.visible: brand.hovered
        ToolTip.text: root.store.languagePairLabel
        ToolTip.delay: AppMetrics.tooltipDelay

        onClicked: languageSwitch.open()

        DesktopLanguagePopover
        {
            id: languageSwitch

            objectName: "language_switch"

            store: root.store
            installedOnly: true
            y: brand.height + AppMetrics.gapTight
            width: AppMetrics.settingsControlWidth
        }

        contentItem: Item
        {
            DesktopKeyHint
            {
                anchors.centerIn: parent
                visible: brand.showsShortcut
                key: "0"
                tint: AppTheme.colourText
            }

            Rectangle
            {
                anchors.centerIn: parent
                visible: !brand.showsShortcut
                width: AppMetrics.headerIconTapWidth
                height: AppMetrics.headerIconTapWidth
                radius: AppMetrics.radiusMedium
                color: AppTheme.colourText

                DesktopLabel
                {
                    anchors.centerIn: parent
                    text: root.store.targetLanguageCode.toUpperCase()
                    font.pixelSize: AppMetrics.fontSmall
                    font.weight: Font.Bold
                    color: AppTheme.colourPanel
                }
            }
        }

        background: Rectangle
        {
            radius: AppMetrics.radiusMedium
            color: root.destination === "account" ? AppTheme.colourFaint
                                                  : (brand.hovered ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0))

            DesktopFocusRing
            {
                visible: brand.visualFocus
            }
        }

        HoverHandler
        {
            cursorShape: Qt.PointingHandCursor
        }
    }

    RowLayout
    {
        id: crumbRow

        anchors.left: parent.left
        anchors.leftMargin: Math.max(AppMetrics.contentPadding, brand.x + brand.width + AppMetrics.gapMedium)
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(0, navRow.x - AppMetrics.gapMedium - anchors.leftMargin)
        spacing: AppMetrics.gapTight
        clip: true
        transform: Translate { id: crumbShift }

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Breadcrumb")

        Repeater
        {
            model: root.crumbs

            delegate: RowLayout
            {
                id: crumb

                required property var modelData
                required property int index

                readonly property bool last: crumb.index === root.crumbs.length - 1

                Layout.minimumWidth: 0
                spacing: AppMetrics.gapTight

                DesktopIcon
                {
                    visible: crumb.index > 0
                    name: "chevron_right"
                    width: AppMetrics.iconSizeSmall
                    height: AppMetrics.iconSizeSmall
                    color: AppTheme.colourMuted
                }

                Button
                {
                    id: crumbButton

                    objectName: "crumb_" + crumb.index

                    Layout.fillWidth: crumb.last
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: crumb.last ? AppMetrics.contentWidthNarrow : AppMetrics.popoverWidth
                    implicitHeight: AppMetrics.buttonHeight
                    leftPadding: AppMetrics.gapSmall
                    rightPadding: AppMetrics.gapSmall
                    topPadding: 0
                    bottomPadding: 0

                    enabled: !crumb.last
                    hoverEnabled: true
                    focusPolicy: crumb.last ? Qt.NoFocus : Qt.StrongFocus
                    text: crumb.modelData

                    Accessible.role: crumb.last ? Accessible.Heading : Accessible.Link
                    Accessible.name: text

                    onClicked: root.crumbActivated(crumb.index)

                    contentItem: DesktopLabel
                    {
                        text: crumbButton.text
                        font.pixelSize: crumb.last ? AppMetrics.fontSubtitle : AppMetrics.fontLabel
                        font.weight: crumb.last ? Font.DemiBold : Font.Medium
                        color: crumb.last ? AppTheme.colourText
                                          : (crumbButton.hovered ? AppTheme.colourText : AppTheme.colourMuted)
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle
                    {
                        radius: AppMetrics.radiusSmall
                        color: crumbButton.hovered ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0)

                        DesktopFocusRing
                        {
                            visible: crumbButton.visualFocus
                            baseRadius: AppMetrics.radiusSmall
                        }
                    }

                    HoverHandler
                    {
                        enabled: !crumb.last
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }
    }

    RowLayout
    {
        id: navRow

        anchors.centerIn: parent
        spacing: AppMetrics.gapMedium

        Accessible.role: Accessible.PageTabList
        Accessible.name: qsTr("Navigation")

        Repeater
        {
            model: root.destinations

            delegate: DesktopNavItem
            {
                id: navItem

                required property var modelData
                required property int index

                objectName: "nav_" + modelData.key

                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.navItemHeight
                z: navItem.index === root.dragIndex ? 1 : 0
                text: modelData.label
                iconName: modelData.icon
                shortcutKey: String(navItem.index + 1)
                collapsed: true
                selected: root.destination === modelData.key
                count: modelData.key === "today" ? root.dueCount : 0
                emphasiseCount: modelData.key === "today"

                transform: Translate
                {
                    x: root.slotOffset(navItem.index)

                    Behavior on x
                    {
                        enabled: root.reordering && navItem.index !== root.dragIndex

                        NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
                    }
                }

                onClicked: root.destinationSelected(modelData.key)

                DragHandler
                {
                    id: reorder

                    target: null
                    yAxis.enabled: false
                    cursorShape: Qt.ClosedHandCursor

                    onActiveChanged:
                    {
                        if (reorder.active)
                        {
                            root.dragIndex = navItem.index
                            root.dragShift = 0
                            root.reordering = true

                            return
                        }

                        Qt.callLater(root.finishDrag)
                    }

                    onActiveTranslationChanged:
                    {
                        if (!reorder.active)
                        {
                            return
                        }

                        const travel = reorder.centroid.scenePosition.x - reorder.centroid.scenePressPosition.x
                        const lowest = -navItem.index * root.slotWidth
                        const highest = (root.destinations.length - 1 - navItem.index) * root.slotWidth

                        root.dragShift = Math.max(lowest, Math.min(highest, travel))
                    }
                }
            }
        }
    }

    readonly property real actionsSpace: root.width - actionRow.anchors.rightMargin
                                         - (navRow.x + navRow.width + AppMetrics.gapMedium)

    readonly property real actionsFullWidth:
    {
        let total = 0
        let count = 0

        for (const child of actionRow.children)
        {
            if (!child.visible)
            {
                continue
            }

            total += child.fullWidth !== undefined ? child.fullWidth : child.implicitWidth
            count += 1
        }

        return total + Math.max(0, count - 1) * actionRow.spacing
    }

    readonly property bool actionsCrowded: root.actionsFullWidth > root.actionsSpace

    function compactActions()
    {
        for (const child of actionRow.children)
        {
            if (child.iconOnly !== undefined)
            {
                child.iconOnly = root.actionsCrowded
            }
        }
    }

    onActionsCrowdedChanged: root.compactActions()
    Component.onCompleted: root.compactActions()

    RowLayout
    {
        id: actionRow

        anchors.right: parent.right
        anchors.rightMargin: root.width - systemRow.x + AppMetrics.gapMedium
        anchors.verticalCenter: parent.verticalCenter
        spacing: AppMetrics.gapSmall
    }

    RowLayout
    {
        id: systemRow

        anchors.right: parent.right
        anchors.rightMargin: AppMetrics.gapMedium
        anchors.verticalCenter: parent.verticalCenter
        spacing: AppMetrics.gapTight

        DesktopNavItem
        {
            objectName: "nav_settings"

            Layout.fillWidth: false
            Layout.preferredWidth: AppMetrics.navItemHeight
            collapsed: true
            text: qsTr("Settings")
            iconName: "nav_settings"
            shortcutKey: "6"
            selected: root.destination === "settings"
            marked: AppSyncController.needsReconnect
                    || (AppCatalog.updateAvailable && !AppUpdater.storePackage)
                    || AppCatalog.packageUpdates.length > 0
            markColour: AppSyncController.needsReconnect ? AppTheme.colourWrong : AppTheme.colourAccent

            onClicked: root.destinationSelected("settings")
        }
    }
}
