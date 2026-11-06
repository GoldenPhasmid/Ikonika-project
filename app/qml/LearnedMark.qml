import QtQuick
import QtQuick.Controls

/*
 * Marks a word the user is actually learning: one on the box ladder, or one finished with. Used on
 * the word being added, on each suggested translation, and on every row of My Vocabulary, so one symbol
 * means the same thing in all of them.
 *
 * A card waiting in the pool is not marked. It has been added and nothing more - starting a
 * session on it changes nothing about the card, since only finishing one promotes anything, so a
 * word part-way through training is indistinguishable from one never opened. Marking those made
 * the symbol mean "there is a card somewhere", which is not what anyone reads it as. Somewhere
 * that wants to say a word has already been added says so in words instead; the add dialog does.
 *
 * A dot rather than a tick: a tick reads as "done correctly", and this is a state the user is in,
 * not the outcome of anything they just did.
 *
 * Large enough, the dot carries the box number instead of leaving the interval to be written out
 * beside it. That only works past the size a digit is legible at, so the small inline form stays
 * plain and says the same thing in its tooltip.
 */
Item
{
    id: root

    /** -1 not a card, 0 waiting in the pool, otherwise the box it reached. */
    required property int stage

    /** True once the word is finished with, whatever box it stopped on. A finished word keeps the
        box it reached, so the two are not the same question. */
    property bool completed: false

    /** 8 is the inline form beside a word or a chip. Around 18 is where a digit inside it becomes
        legible, which is what the vocabulary list uses. */
    property int size: 8

    /** Whether there is anything to mark. Everything below is conditioned on this rather than on
        the item's own visible, which a caller can and does replace with a binding of its own: the
        vocabulary list hides the mark to make room for a row's buttons, and that binding silently
        took the place of this rule and put a dot on every word. */
    readonly property bool learning: root.stage > 0 || root.completed

    /** Below this a numeral does not survive being drawn, so it is left off and the tooltip is
        the only place the box is named. */
    readonly property bool showsNumber: root.size >= 16 && root.stage > 0 && !root.completed

    // No space taken when there is nothing to mark, so a row of words does not carry a column of
    // gaps where the untrained ones are.
    implicitWidth: root.learning ? root.size : 0
    implicitHeight: root.learning ? root.size : 0

    visible: root.learning

    Rectangle
    {
        anchors.centerIn: parent

        visible: root.learning

        width: root.size
        height: root.size
        radius: width / 2

        // A finished word is the one state that is not about progress, so it leaves the accent
        // behind for the colour a right answer uses everywhere else. It also carries no number,
        // which is what keeps it apart from the last box without relying on the colour.
        color: root.completed ? AppTheme.colourCorrect : AppTheme.colourAccent

        Text
        {
            anchors.centerIn: parent
            visible: root.showsNumber

            text: root.stage
            color: AppTheme.colourAccentText

            // Sized against the dot rather than the page, so the digit still fits when the mark
            // is made bigger or smaller.
            font.pixelSize: Math.max(9, Math.round(root.size * 0.62))
            font.bold: true
        }
    }

    HoverHandler
    {
        id: hover
    }

    ToolTip
    {
        visible: hover.hovered && root.learning
        delay: 300

        text: root.completed
            ? qsTr("Learned")
            : qsTr("Learning, at interval %1").arg(root.stage)
    }
}
