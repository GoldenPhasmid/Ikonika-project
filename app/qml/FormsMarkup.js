.pragma library

function colourEndings(markup, colour)
{
    return markup.replace(/<a href="ending">/g, "<font color=\"" + colour + "\">").replace(/<\/a>/g, "</font>")
}
