.pragma library

function list()
{
    return [
        { key: "paper", label: qsTr("Paper"), paper: "#FFFFFF", ink: "#000000" },
        { key: "sepia", label: qsTr("Sepia"), paper: "#F4ECD8", ink: "#3B2F24" },
        { key: "night", label: qsTr("Night"), paper: "#1C1C1C", ink: "#D0D0D0" }
    ]
}

function find(key)
{
    const tones = list()

    return tones.find(function(tone) { return tone.key === key }) || tones[0]
}

function textList()
{
    return [{ key: "theme", label: qsTr("Theme"), paper: "", ink: "" }].concat(list())
}

function findText(key)
{
    const tones = textList()

    return tones.find(function(tone) { return tone.key === key }) || tones[0]
}
