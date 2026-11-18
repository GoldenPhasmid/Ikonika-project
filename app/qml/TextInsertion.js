.pragma library

function insertAtCursor(input, text)
{
    if (input.selectedText.length > 0)
    {
        input.remove(input.selectionStart, input.selectionEnd)
    }

    input.insert(input.cursorPosition, text)
    input.forceActiveFocus()
}
