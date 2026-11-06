import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

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

    readonly property bool updateFailed: AppCatalog.updateAvailable && AppUpdater.lastError.length > 0

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

    function updateStatus()
    {
        if (AppUpdater.storePackage)
        {
            return qsTr("The Microsoft Store delivers updates.")
        }

        if (AppUpdater.ready)
        {
            return qsTr("Version %1 is ready to install.").arg(AppCatalog.publishedVersion)
        }

        if (AppUpdater.busy)
        {
            return qsTr("Downloading version %1, %2 of %3 MB").arg(AppCatalog.publishedVersion)
                .arg(root.megabytes(AppUpdater.receivedBytes)).arg(root.megabytes(AppUpdater.totalBytes))
        }

        if (root.updateFailed)
        {
            return qsTr("Version %1 could not be downloaded: %2").arg(AppCatalog.publishedVersion)
                .arg(AppUpdater.lastError)
        }

        if (AppCatalog.updateAvailable)
        {
            return qsTr("Version %1 is available.").arg(AppCatalog.publishedVersion)
        }

        if (AppCatalog.busy)
        {
            return qsTr("Checking for updates")
        }

        if (AppCatalog.loaded)
        {
            return qsTr("Ikonika is up to date.")
        }

        if (AppCatalog.lastError.length > 0)
        {
            return qsTr("Updates could not be checked: %1").arg(AppCatalog.lastError)
        }

        return ""
    }

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    DesktopSettingsGroup
    {
        title: qsTr("App")

        DesktopSettingsRow
        {
            text: qsTr("Version")

            DesktopLabel
            {
                text: Qt.application.version
                color: AppTheme.colourSubtle
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Updates")
            description: root.updateStatus()

            DesktopButton
            {
                objectName: "app_update_check"

                visible: !AppUpdater.storePackage
                enabled: !AppCatalog.busy
                text: qsTr("Check for updates")

                onClicked: AppCatalog.Refresh()
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Privacy policy")
            clickable: true
            lastRow: true

            onClicked: Qt.openUrlExternally("https://goldenphasmid.github.io/ikonika-releases/privacy")

            DesktopIcon
            {
                name: "external_link"
                width: AppMetrics.iconSizeSmall
                height: AppMetrics.iconSizeSmall
                color: AppTheme.colourMuted
            }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Credits")

        Repeater
        {
            model: root.credits

            delegate: DesktopSettingsRow
            {
                required property var modelData
                required property int index

                text: modelData.title
                description: modelData.description
                clickable: true
                lastRow: index === root.credits.length - 1

                onClicked: Qt.openUrlExternally(modelData.url)

                DesktopIcon
                {
                    name: "external_link"
                    width: AppMetrics.iconSizeSmall
                    height: AppMetrics.iconSizeSmall
                    color: AppTheme.colourMuted
                }
            }
        }
    }
}
