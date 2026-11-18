import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property string section: "all"
    property var counts: ({})
    property var collections: []
    property bool freeShown: false
    property real reveal: 0

    signal sectionChosen(string section)
    signal newCollectionRequested()
    signal collectionHeld(string collectionId, string name)
    signal importRequested()

    function open()
    {
        root.visible = true
        hideAnimation.stop()
        showAnimation.restart()
    }

    function close()
    {
        showAnimation.stop()
        hideAnimation.restart()
    }

    function choose(section)
    {
        root.sectionChosen(section)
        root.close()
    }

    visible: false

    NumberAnimation
    {
        id: showAnimation

        target: root
        property: "reveal"
        to: 1
        duration: AppMetrics.motionSlow
        easing.type: AppMetrics.motionEasing
    }

    NumberAnimation
    {
        id: hideAnimation

        target: root
        property: "reveal"
        to: 0
        duration: AppMetrics.motionBase
        easing.type: AppMetrics.motionEasing

        onFinished: root.visible = false
    }

    component SectionRow: Rectangle
    {
        id: row

        required property string key
        property string label: ""
        property string iconName: ""
        property color dotColour: "transparent"
        property int count: root.counts[row.key] || 0

        signal held()

        readonly property bool selected: root.section === row.key

        Layout.fillWidth: true
        implicitHeight: AppMetrics.buttonHeightLarge
        radius: height / 2
        color: row.selected ? AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaMedium)
                            : rowTap.pressed ? AppTheme.colourFaint : "transparent"

        objectName: "reader_section_" + row.key

        Accessible.role: Accessible.Button
        Accessible.name: qsTr("%1, %2").arg(row.label).arg(row.count)
        Accessible.selected: row.selected

        RowLayout
        {
            anchors.fill: parent
            anchors.leftMargin: AppMetrics.contentPadding
            anchors.rightMargin: AppMetrics.contentPadding
            spacing: AppMetrics.gapMedium

            DesktopIcon
            {
                visible: row.iconName.length > 0
                name: row.iconName
                width: AppMetrics.headerActionIconSize
                height: AppMetrics.headerActionIconSize
                color: row.selected ? AppTheme.colourText : AppTheme.colourSubtle
            }

            Rectangle
            {
                visible: row.iconName.length === 0
                Layout.preferredWidth: AppMetrics.headerActionIconSize
                Layout.preferredHeight: AppMetrics.headerActionIconSize

                color: "transparent"

                Rectangle
                {
                    anchors.centerIn: parent
                    width: AppMetrics.categoryDotSize
                    height: width
                    radius: AppMetrics.radiusSmall / 2
                    color: row.dotColour
                }
            }

            Text
            {
                Layout.fillWidth: true
                text: row.label
                font.pixelSize: AppMetrics.fontLabel
                font.weight: row.selected ? Font.DemiBold : Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                elide: Text.ElideRight
            }

            Text
            {
                text: row.count
                font.pixelSize: AppMetrics.fontSmall
                font.family: AppMetrics.fontFamily
                font.features: { "tnum": 1 }
                color: row.selected ? AppTheme.colourText : AppTheme.colourMuted
            }
        }

        TapHandler
        {
            id: rowTap

            onTapped: root.choose(row.key)
            onLongPressed: row.held()
        }
    }

    component Heading: Text
    {
        Layout.fillWidth: true
        Layout.leftMargin: AppMetrics.contentPadding
        Layout.topMargin: AppMetrics.gapSmall
        Layout.bottomMargin: AppMetrics.gapTight
        font.pixelSize: AppMetrics.fontMicro
        font.weight: Font.DemiBold
        font.family: AppMetrics.fontFamily
        font.letterSpacing: AppMetrics.trackingWide
        font.capitalization: Font.AllUppercase
        color: AppTheme.colourMuted
    }

    Rectangle
    {
        anchors.fill: parent
        color: AppTheme.colourScrim
        opacity: root.reveal

        MouseArea
        {
            anchors.fill: parent

            onClicked: root.close()
        }
    }

    Rectangle
    {
        id: panel

        x: -width * (1 - root.reveal)
        width: Math.min(root.width - AppMetrics.buttonHeightLarge * 2, AppMetrics.categoryPaneWidth * 1.4)
        height: root.height
        color: AppTheme.colourPanel
        topRightRadius: AppMetrics.radiusLarge
        bottomRightRadius: AppMetrics.radiusLarge

        Accessible.role: Accessible.PageTabList
        Accessible.name: qsTr("Library sections")

        MouseArea
        {
            anchors.fill: parent
        }

        ColumnLayout
        {
            anchors.fill: parent
            anchors.topMargin: root.SafeArea.margins.top + AppMetrics.gapMedium
            anchors.bottomMargin: root.SafeArea.margins.bottom + AppMetrics.contentPadding
            anchors.leftMargin: AppMetrics.gapSmall
            anchors.rightMargin: AppMetrics.gapSmall
            spacing: 0

            Heading
            {
                text: qsTr("Library")
            }

            SectionRow
            {
                key: "all"
                label: qsTr("My books")
                iconName: "library"
            }

            SectionRow
            {
                visible: root.freeShown
                key: "free"
                label: qsTr("Free library")
                iconName: "globe"
            }

            SectionRow
            {
                key: "reading"
                label: qsTr("Reading")
                iconName: "nav_reader"
            }

            SectionRow
            {
                key: "finished"
                label: qsTr("Finished")
                iconName: "circle_check"
            }

            SectionRow
            {
                key: "favorites"
                label: qsTr("Favorites")
                iconName: "heart"
            }

            Rectangle
            {
                Layout.fillWidth: true
                Layout.leftMargin: AppMetrics.contentPadding
                Layout.rightMargin: AppMetrics.contentPadding
                Layout.topMargin: AppMetrics.gapSmall
                Layout.bottomMargin: AppMetrics.gapSmall
                Layout.preferredHeight: AppMetrics.borderWidth
                color: AppTheme.colourBorder
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: 0

                Heading
                {
                    Layout.topMargin: 0
                    Layout.bottomMargin: 0
                    text: qsTr("Collections")
                }

                MobileIconButton
                {
                    objectName: "reader_new_collection"

                    iconName: "plus"
                    iconSize: AppMetrics.fontLabel
                    iconColour: AppTheme.colourSubtle
                    text: qsTr("New collection")

                    onClicked: root.newCollectionRequested()
                }
            }

            Flickable
            {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: collectionColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                ColumnLayout
                {
                    id: collectionColumn

                    width: parent.width
                    spacing: 0

                    Repeater
                    {
                        model: root.collections

                        delegate: SectionRow
                        {
                            required property var modelData

                            key: "collection:" + modelData.id
                            label: modelData.name
                            dotColour: AppTheme.coverColours[modelData.colour % AppTheme.coverColours.length]
                            count: modelData.count

                            onHeld: root.collectionHeld(modelData.id, modelData.name)
                        }
                    }
                }
            }

            MobileWideButton
            {
                objectName: "reader_import"

                Layout.leftMargin: AppMetrics.gapSmall
                Layout.rightMargin: AppMetrics.gapSmall
                label: qsTr("Import a book")

                onClicked:
                {
                    root.close()
                    root.importRequested()
                }
            }
        }
    }
}
