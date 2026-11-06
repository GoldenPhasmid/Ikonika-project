import QtQuick

Item
{
    id: root

    property bool morphing: false
    property real shelfShown: 1
    property real shelfScale: 1
    property real pageShown: 1
    property real chromeShown: 1
    property real faceShown: 1
    property real backdropShown: 0

    property rect ghostArea
    property rect morphFrom
    property rect morphTo
    property rect backdropArea
    property color backdropColour: "transparent"
    property real titleSize: AppMetrics.fontSubtitle
    property real coverRadius: AppMetrics.coverRadius
    property real spineShown: 0

    property bool contentReady: true
    property bool awaitingContent: false

    property Item landingTile: null
    property real landingProgress: -1
    property var afterShut: null
    property var findHome: null

    readonly property bool available: AppMetrics.motionMorph > 0 && !root.morphing

    signal openStarted()

    function rectIn(item)
    {
        return item.mapToItem(root, 0, 0, item.width, item.height)
    }

    function copyFace(tile)
    {
        face.title = tile.title
        face.format = tile.format
        face.placeIcon = tile.placeIcon
        face.coverColour = tile.coverColour
        face.inkColour = tile.inkColour
        face.progressShown = tile.progressShown
        face.progress = Math.max(0, tile.readProgress)
        root.landingProgress = tile.readProgress
    }

    function open(tile, landing)
    {
        if (!root.available || !tile)
        {
            return false
        }

        root.copyFace(tile)
        root.morphFrom = root.rectIn(tile.face)
        root.morphTo = landing
        root.ghostArea = root.morphFrom
        root.titleSize = AppMetrics.fontSubtitle
        root.coverRadius = AppMetrics.coverRadius
        root.spineShown = 0
        root.faceShown = 1
        root.shelfShown = 1
        root.shelfScale = 1
        root.pageShown = 0
        root.chromeShown = 0
        root.backdropShown = 0
        root.landingTile = tile
        tile.morphHidden = true
        root.morphing = true

        console.info("Book", tile.title, "grows from its cover")
        growAnimation.start()
        root.openStarted()

        return true
    }

    function afterGrow()
    {
        if (!root.contentReady)
        {
            root.awaitingContent = true

            return
        }

        revealAnimation.start()
    }

    function finishOpen()
    {
        growAnimation.stop()
        revealAnimation.stop()
        root.awaitingContent = false

        if (root.landingTile)
        {
            root.landingTile.morphHidden = false
        }

        root.landingTile = null
        root.resetStage()
        root.morphing = false
    }

    function close(tile, landing, page, pageColour, shut, home)
    {
        if (!root.available || !tile)
        {
            return false
        }

        root.copyFace(tile)
        root.ghostArea = landing
        root.backdropArea = page
        root.backdropColour = pageColour
        root.titleSize = AppMetrics.bookTitleSize
        root.coverRadius = AppMetrics.bookRadius
        root.spineShown = 1
        root.faceShown = 0
        root.shelfShown = 0
        root.shelfScale = 1
        root.pageShown = 1
        root.chromeShown = 1
        root.backdropShown = 0
        root.afterShut = shut
        root.findHome = home
        root.morphing = true

        console.info("Book", tile.title, "shrinks onto its cover")
        coverAnimation.start()

        return true
    }

    function afterCover()
    {
        const shut = root.afterShut

        root.afterShut = null
        root.backdropShown = 1

        if (shut)
        {
            shut()
        }

        Qt.callLater(root.flyHome)
    }

    function flyHome()
    {
        const tile = root.findHome ? root.findHome() : null

        root.findHome = null

        if (!tile)
        {
            console.warn("Book morph found no cover to land on")
            root.finishClose()

            return
        }

        tile.morphHidden = true
        tile.heldProgress = root.landingProgress
        root.landingTile = tile
        root.morphFrom = root.ghostArea
        root.morphTo = root.rectIn(tile.face)
        homeAnimation.start()
    }

    function finishClose()
    {
        coverAnimation.stop()
        homeAnimation.stop()

        if (root.landingTile)
        {
            root.landingTile.morphHidden = false
            root.landingTile.heldProgress = -1
        }

        root.landingTile = null
        root.resetStage()
        root.morphing = false
    }

    function resetStage()
    {
        root.faceShown = 1
        root.shelfShown = 1
        root.shelfScale = 1
        root.pageShown = 1
        root.chromeShown = 1
        root.backdropShown = 0
    }

    onContentReadyChanged:
    {
        if (root.awaitingContent && root.contentReady)
        {
            root.awaitingContent = false
            revealAnimation.start()
        }
    }

    MouseArea
    {
        anchors.fill: parent
        visible: root.morphing
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons

        onWheel: function(wheel) { wheel.accepted = true }
    }

    Rectangle
    {
        x: root.backdropArea.x
        y: root.backdropArea.y
        width: root.backdropArea.width
        height: root.backdropArea.height
        visible: root.morphing && root.backdropShown > 0
        opacity: root.backdropShown
        color: root.backdropColour
    }

    DesktopBookCover
    {
        id: face

        x: root.ghostArea.x
        y: root.ghostArea.y
        width: root.ghostArea.width
        height: root.ghostArea.height
        visible: root.morphing && root.faceShown > 0
        opacity: root.faceShown

        radius: root.coverRadius
        titleSize: root.titleSize
        spineShown: root.spineShown
    }

    ParallelAnimation
    {
        id: growAnimation

        PropertyAnimation { target: root; property: "ghostArea"; from: root.morphFrom; to: root.morphTo; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: root; property: "coverRadius"; to: AppMetrics.bookRadius; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: root; property: "titleSize"; to: AppMetrics.bookTitleSize; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: root; property: "shelfShown"; to: 0; duration: AppMetrics.motionMorph / 2; easing.type: Easing.InCubic }
        NumberAnimation { target: root; property: "shelfScale"; to: AppMetrics.shelfRecedeScale; duration: AppMetrics.motionMorph / 2; easing.type: Easing.InCubic }
        NumberAnimation { target: root; property: "pageShown"; to: 1; duration: AppMetrics.motionMorph }

        SequentialAnimation
        {
            PauseAnimation { duration: AppMetrics.motionMorph * 0.4 }
            NumberAnimation { target: root; property: "spineShown"; to: 1; duration: AppMetrics.motionMorph * 0.6 }
        }

        onFinished: root.afterGrow()
    }

    ParallelAnimation
    {
        id: revealAnimation

        NumberAnimation { target: root; property: "chromeShown"; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }

        SequentialAnimation
        {
            PauseAnimation { duration: AppMetrics.motionBase / 2 }
            NumberAnimation { target: root; property: "faceShown"; to: 0; duration: AppMetrics.motionBase }
        }

        onFinished: root.finishOpen()
    }

    ParallelAnimation
    {
        id: coverAnimation

        NumberAnimation { target: root; property: "chromeShown"; to: 0; duration: AppMetrics.motionBase * 0.75 }

        SequentialAnimation
        {
            PauseAnimation { duration: AppMetrics.motionBase * 0.75 }
            NumberAnimation { target: root; property: "faceShown"; to: 1; duration: AppMetrics.motionBase }
        }

        onFinished: root.afterCover()
    }

    ParallelAnimation
    {
        id: homeAnimation

        PropertyAnimation { target: root; property: "ghostArea"; from: root.morphFrom; to: root.morphTo; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: root; property: "coverRadius"; to: AppMetrics.coverRadius; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: root; property: "titleSize"; to: AppMetrics.fontSubtitle; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: root; property: "spineShown"; to: 0; duration: AppMetrics.motionMorph / 2 }
        NumberAnimation { target: root; property: "backdropShown"; to: 0; duration: AppMetrics.motionMorph * 0.6 }

        SequentialAnimation
        {
            PauseAnimation { duration: AppMetrics.motionMorph * 0.3 }
            NumberAnimation { target: root; property: "shelfShown"; to: 1; duration: AppMetrics.motionMorph * 0.7; easing.type: AppMetrics.motionEasing }
        }

        onFinished: root.finishClose()
    }
}
