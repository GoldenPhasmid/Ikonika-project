import QtQuick

Item
{
    id: root

    property bool running: false
    property real shelfShown: 1
    property real shelfScale: 1
    property real pageShown: 1
    property real chromeShown: 1

    property bool contentReady: true
    property bool awaitingContent: false
    property bool opening: false
    property var afterShut: null

    readonly property bool available: AppMetrics.motionBase > 0 && !root.running

    signal openStarted()

    function open()
    {
        if (!root.available)
        {
            return false
        }

        root.shelfShown = 1
        root.shelfScale = 1
        root.pageShown = 0
        root.chromeShown = 0
        root.running = true
        root.opening = true

        leaveShelfAnimation.start()
        root.openStarted()

        return true
    }

    function afterLeaveShelf()
    {
        if (!root.contentReady)
        {
            root.awaitingContent = true

            return
        }

        showPageAnimation.start()
    }

    function finishOpen()
    {
        leaveShelfAnimation.stop()
        showPageAnimation.stop()
        root.awaitingContent = false
        root.opening = false
        root.resetStage()
        root.running = false
    }

    function close(shut)
    {
        if (!root.available)
        {
            return false
        }

        root.shelfShown = 0
        root.shelfScale = AppMetrics.shelfRecedeScale
        root.pageShown = 1
        root.chromeShown = 1
        root.afterShut = shut
        root.running = true

        hidePageAnimation.start()

        return true
    }

    function afterHidePage()
    {
        const shut = root.afterShut

        root.afterShut = null

        if (shut)
        {
            shut()
        }

        showShelfAnimation.start()
    }

    function finishClose()
    {
        hidePageAnimation.stop()
        showShelfAnimation.stop()
        root.resetStage()
        root.running = false
    }

    function resetStage()
    {
        root.shelfShown = 1
        root.shelfScale = 1
        root.pageShown = 1
        root.chromeShown = 1
    }

    onContentReadyChanged:
    {
        if (root.awaitingContent && root.contentReady)
        {
            root.awaitingContent = false
            showPageAnimation.start()
        }
    }

    MouseArea
    {
        anchors.fill: parent
        visible: root.running
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons

        onWheel: function(wheel) { wheel.accepted = true }
    }

    ParallelAnimation
    {
        id: leaveShelfAnimation

        NumberAnimation { target: root; property: "shelfShown"; to: 0; duration: AppMetrics.motionBase; easing.type: Easing.InCubic }
        NumberAnimation { target: root; property: "shelfScale"; to: AppMetrics.shelfRecedeScale; duration: AppMetrics.motionBase; easing.type: Easing.InCubic }

        onFinished: root.afterLeaveShelf()
    }

    ParallelAnimation
    {
        id: showPageAnimation

        NumberAnimation { target: root; property: "pageShown"; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: root; property: "chromeShown"; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }

        onFinished: root.finishOpen()
    }

    ParallelAnimation
    {
        id: hidePageAnimation

        NumberAnimation { target: root; property: "pageShown"; to: 0; duration: AppMetrics.motionBase; easing.type: Easing.InCubic }
        NumberAnimation { target: root; property: "chromeShown"; to: 0; duration: AppMetrics.motionBase; easing.type: Easing.InCubic }

        onFinished: root.afterHidePage()
    }

    ParallelAnimation
    {
        id: showShelfAnimation

        NumberAnimation { target: root; property: "shelfShown"; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: root; property: "shelfScale"; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }

        onFinished: root.finishClose()
    }
}
