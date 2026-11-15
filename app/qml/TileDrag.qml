import QtQuick

Item
{
    id: root

    property var controller: null
    property Item answerArea: null
    property Repeater answerRepeater: null
    property AppStackingItem gap: null
    property Item endItem: null
    property real slack: 0

    property int tileIndex: -1
    property Item source: null
    property bool fromAnswer: false
    property bool overAnswer: false
    property int insertIndex: -1
    property bool gapShown: false
    property string text: ""
    property real ghostWidth: 0
    property real ghostX: 0
    property real ghostY: 0
    property point pressOffset: Qt.point(0, 0)

    readonly property bool active: root.tileIndex >= 0
    readonly property bool endTarget: root.overAnswer && !root.gapShown

    signal gapMoved()

    function begin(source, tileIndex, text, fromAnswer, pressPosition, scenePosition)
    {
        root.ghostWidth = source.width
        root.source = source
        root.text = text
        root.fromAnswer = fromAnswer
        root.pressOffset = pressPosition
        root.insertIndex = -1
        root.tileIndex = tileIndex
        root.move(scenePosition)
    }

    function otherTiles()
    {
        const tiles = []

        for (let index = 0; index < root.answerRepeater.count; ++index)
        {
            const tile = root.answerRepeater.itemAt(index)

            if (tile && tile !== root.source)
            {
                tiles.push(tile)
            }
        }

        return tiles
    }

    function isOverAnswer(scenePosition)
    {
        if (!root.fromAnswer && root.controller.canSubmit)
        {
            return false
        }

        const point = root.answerArea.mapFromItem(null, scenePosition.x, scenePosition.y)

        return point.x >= -root.slack && point.x <= root.answerArea.width + root.slack
            && point.y >= -root.slack && point.y <= root.answerArea.height + root.slack
    }

    function findInsertIndex(tiles, scenePosition)
    {
        const point = root.endItem.parent.mapFromItem(null, scenePosition.x, scenePosition.y)

        let rowY = 0
        let rowDistance = Infinity

        for (const tile of tiles)
        {
            const distance = Math.max(tile.y - point.y, point.y - tile.y - tile.height, 0)

            if (distance < rowDistance)
            {
                rowDistance = distance
                rowY = tile.y
            }
        }

        for (let index = 0; index < tiles.length; ++index)
        {
            const tile = tiles[index]
            const before = tile.y < rowY || (tile.y === rowY && point.x > tile.x + tile.width / 2)

            if (!before)
            {
                return index
            }
        }

        return tiles.length
    }

    function move(scenePosition)
    {
        if (!root.active)
        {
            return
        }

        const local = root.mapFromItem(null, scenePosition.x, scenePosition.y)
        root.ghostX = local.x - root.pressOffset.x
        root.ghostY = local.y - root.pressOffset.y

        root.overAnswer = root.isOverAnswer(scenePosition)

        if (!root.overAnswer)
        {
            root.insertIndex = -1

            return
        }

        const tiles = root.otherTiles()
        const index = root.findInsertIndex(tiles, scenePosition)

        if (index === root.insertIndex)
        {
            return
        }

        root.insertIndex = index
        root.gapShown = index < tiles.length
        root.gap.placeBefore(root.gapShown ? tiles[index] : root.endItem)
        root.gapMoved()
    }

    function finish()
    {
        if (!root.active)
        {
            return
        }

        const controller = root.controller
        const tileIndex = root.tileIndex
        const position = root.insertIndex
        const dropped = root.overAnswer
        const fromAnswer = root.fromAnswer

        console.info("TileDrag: tile", tileIndex, dropped ? "dropped at " + position : "dropped outside the answer")

        root.tileIndex = -1
        root.source = null
        root.overAnswer = false
        root.insertIndex = -1

        Qt.callLater(function()
        {
            if (dropped)
            {
                controller.insertTile(tileIndex, position)
            }
            else if (fromAnswer)
            {
                controller.removeTile(tileIndex)
            }
        })
    }

    z: 1000
}
