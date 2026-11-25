import QtQuick

QtObject
{
    property bool started: false
    property bool playing: false
    property bool available: false
    property int paragraph: 0
    property int sentenceStart: 0
    property int sentenceEnd: 0
    property int savedParagraph: -1
    property int savedCharacter: 0
    signal stateChanged()
}
