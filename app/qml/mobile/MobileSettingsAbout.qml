import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    MobileSettingsGroup
    {
        title: qsTr("APP")

        MobileSettingsRow
        {
            title: qsTr("Version")

            MobileSettingsValue
            {
                text: Qt.application.version
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Privacy policy")
            lastRow: true

            onClicked: Qt.openUrlExternally("https://goldenphasmid.github.io/ikonika-releases/privacy")

            MobileSettingsChevron {}
        }
    }

    readonly property var credits: [
        { title: qsTr("Wiktionary"),
          description: qsTr("Words, meanings, genders and forms by the Wiktionary contributors, extracted with Wiktextract (kaikki.org). CC BY-SA and GFDL"),
          url: "https://kaikki.org" },
        { title: qsTr("Tatoeba"),
          description: qsTr("Example sentences and their translations by the Tatoeba contributors. CC BY 2.0 FR"),
          url: "https://tatoeba.org" },
        { title: qsTr("OpenSubtitles"),
          description: qsTr("Translation frequencies from the OpenSubtitles corpus (P. Lison and J. Tiedemann, 2016) distributed by OPUS, built from opensubtitles.org"),
          url: "http://www.opensubtitles.org" },
        { title: qsTr("spaCy"),
          description: qsTr("Lemmas and parts of speech of the example sentences. Explosion, MIT licence"),
          url: "https://spacy.io" },
        { title: qsTr("Lexique"),
          description: qsTr("French word frequency. B. New, C. Pallier et al. CC BY-SA 4.0"),
          url: "http://www.lexique.org" },
        { title: qsTr("EsPal"),
          description: qsTr("Spanish word frequency. A. Duchon et al., 2013"),
          url: "http://www.bcbl.eu/databases/espal" },
        { title: qsTr("SUBTLEX-US"),
          description: qsTr("English word frequency. M. Brysbaert and B. New, 2009"),
          url: "https://www.ugent.be/pp/experimentele-psychologie/en/research/documents/subtlexus" },
        { title: qsTr("FLELex and ELELex"),
          description: qsTr("French and Spanish CEFR levels. T. François et al., CENTAL, UCLouvain"),
          url: "https://cental.uclouvain.be/cefrlex/" },
        { title: qsTr("CEFR-J"),
          description: qsTr("The CEFR-J Wordlist Version 1.5. Compiled by Yukio Tono, Tokyo University of Foreign Studies"),
          url: "https://www.cefr-j.org" },
        { title: qsTr("Octanove"),
          description: qsTr("English C1 and C2 levels, Octanove Vocabulary Profile by Octanove Labs. CC BY-SA 4.0"),
          url: "https://github.com/openlanguageprofiles/olp-en-cefrj" },
        { title: qsTr("Qt"),
          description: qsTr("The app is built with Qt. LGPL v3"),
          url: "https://www.qt.io" },
        { title: qsTr("Inter"),
          description: qsTr("The typeface, by Rasmus Andersson and the Inter Project Authors. SIL Open Font License 1.1"),
          url: "https://rsms.me/inter" }
    ]

    MobileSettingsGroup
    {
        title: qsTr("CREDITS")

        Repeater
        {
            model: root.credits

            delegate: MobileSettingsRow
            {
                required property var modelData
                required property int index

                title: modelData.title
                description: modelData.description
                lastRow: index === root.credits.length - 1

                onClicked: Qt.openUrlExternally(modelData.url)

                MobileSettingsChevron {}
            }
        }
    }
}
