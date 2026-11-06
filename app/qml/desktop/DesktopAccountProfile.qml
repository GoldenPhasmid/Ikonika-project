import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    signal signOutRequested()
    signal eraseRequested()

    readonly property string initials:
    {
        const source = AppSyncController.accountName.length > 0 ? AppSyncController.accountName
                                                                : AppSyncController.accountEmail.split("@")[0]
        const words = source.split(/[ ._\-]/).filter(Word => Word.length > 0)

        if (words.length === 0)
        {
            return ""
        }

        return words.length >= 2 ? (words[0][0] + words[1][0]).toUpperCase() : words[0].substring(0, 2).toUpperCase()
    }

    readonly property string displayName:
    {
        if (!AppSyncController.signedIn)
        {
            return qsTr("Not signed in")
        }

        if (AppSyncController.accountName.length > 0)
        {
            return AppSyncController.accountName
        }

        return AppSyncController.accountEmail.length > 0 ? AppSyncController.accountEmail : qsTr("Signed in")
    }

    readonly property string statusLine:
    {
        if (AppSyncController.signedIn)
        {
            return AppSyncController.statusText
        }

        return AppSyncController.configured ? ""
                                            : qsTr("Sync is not set up on this build")
    }

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    DesktopCard
    {
        Layout.fillWidth: true

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapMedium

            Rectangle
            {
                Layout.preferredWidth: AppMetrics.avatarSize
                Layout.preferredHeight: AppMetrics.avatarSize
                radius: width / 2
                color: AppSyncController.signedIn ? AppTheme.colourAccent : AppTheme.colourFaint

                DesktopLabel
                {
                    anchors.centerIn: parent
                    text: root.initials
                    font.pixelSize: AppMetrics.fontLabel
                    font.weight: Font.Bold
                    color: AppSyncController.signedIn ? AppTheme.colourAccentText : AppTheme.colourMuted
                }

                DesktopIcon
                {
                    anchors.centerIn: parent
                    visible: root.initials.length === 0
                    name: "nav_settings"
                    color: AppTheme.colourMuted
                }
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapTight

                DesktopLabel
                {
                    Layout.fillWidth: true
                    text: root.displayName
                    font.pixelSize: AppMetrics.fontLabel
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    visible: AppSyncController.signedIn && AppSyncController.accountName.length > 0
                    text: AppSyncController.accountEmail
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted
                    elide: Text.ElideRight
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    visible: root.statusLine.length > 0
                    text: root.statusLine
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourSubtle
                    wrapMode: Text.WordWrap
                }
            }

            DesktopButton
            {
                visible: AppSyncController.needsReconnect
                tone: "primary"
                text: qsTr("Reconnect")

                onClicked: AppSyncController.signIn()
            }

            DesktopButton
            {
                visible: AppSyncController.signedIn && !AppSyncController.needsReconnect
                text: AppSyncController.syncing ? qsTr("Syncing…") : qsTr("Sync now")
                enabled: !AppSyncController.syncing

                onClicked: AppSyncController.syncNow()
            }

            DesktopButton
            {
                visible: AppSyncController.signedIn
                text: qsTr("Switch account")
                enabled: AppSyncController.configured && !AppSyncController.busy

                onClicked: AppSyncController.switchAccount()
            }

            DesktopButton
            {
                tone: AppSyncController.signedIn ? "dangerOutline" : "primary"
                text: AppSyncController.signedIn ? qsTr("Sign out") : qsTr("Sign in with Google")
                enabled: AppSyncController.configured

                onClicked: AppSyncController.signedIn ? root.signOutRequested() : AppSyncController.signIn()
            }
        }
    }
}
