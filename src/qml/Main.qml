// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.platform as Platform
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.yuanliou.FlatpakFileInstaller

Kirigami.ApplicationWindow {
    id: root

    title: Installer.state === Installer.State.Empty ? i18n("Flatpak File Installer") : i18n("Install “%1”", Installer.appName)
    width: Kirigami.Units.gridUnit * 38
    height: Kirigami.Units.gridUnit * 36
    minimumWidth: Kirigami.Units.gridUnit * 30
    minimumHeight: Kirigami.Units.gridUnit * 28

    // The main page has its own gear button. Pages on top of it (settings,
    // about) get a toolbar with a back button.
    pageStack.globalToolBar.style: pageStack.layers.depth > 1 ? Kirigami.ApplicationHeaderStyle.ToolBar : Kirigami.ApplicationHeaderStyle.None
    pageStack.initialPage: Kirigami.Page {
        // Not while installing: leaving would hide what is happening.
        header: Item {
            implicitHeight: settingsButton.implicitHeight + Kirigami.Units.smallSpacing

            QQC2.ToolButton {
                id: settingsButton
                anchors.right: parent.right
                anchors.rightMargin: Kirigami.Units.smallSpacing
                anchors.top: parent.top
                anchors.topMargin: Kirigami.Units.smallSpacing
                visible: !Installer.busy
                // No text: some styles draw it next to the icon whatever
                // `display` says. The name is for the tooltip and screen readers.
                display: QQC2.AbstractButton.IconOnly
                // The icon is a plain black shape, painted in the text color.
                contentItem: Kirigami.Icon {
                    source: "qrc:/icons/settings.svg"
                    isMask: true
                    color: Kirigami.Theme.textColor
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                }
                Accessible.name: i18n("Settings")
                QQC2.ToolTip.text: i18n("Settings")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: root.openSettings()
            }
        }

        Loader {
            anchors.fill: parent
            sourceComponent: {
                switch (Installer.state) {
                case Installer.State.Empty:
                    return emptyView;
                case Installer.State.Invalid:
                case Installer.State.FlatpakMissing:
                    return problemView;
                case Installer.State.Checking:
                    return checkingView;
                case Installer.State.Ready:
                    return readyView;
                case Installer.State.AlreadyInstalled:
                    return existingView;
                case Installer.State.Installing:
                    return installingView;
                case Installer.State.Installed:
                    return installedView;
                case Installer.State.Failed:
                    return failedView;
                }
                return emptyView;
            }
        }
    }

    // A file dragged onto the window opens it, whatever is showing, settings
    // and about included. It sits on the overlay, above those pages.
    FileDropArea {
        id: fileDrop
        parent: QQC2.Overlay.overlay
        anchors.fill: parent
        enabled: !Installer.busy
        onFileDropped: file => {
            // An invalid file doesn't make the installer busy, so the pages
            // on top are not cleared by the state change.
            root.pageStack.layers.clear();
            Installer.open(file);
        }
    }

    Rectangle {
        parent: QQC2.Overlay.overlay
        anchors.fill: parent
        visible: fileDrop.containsDrag
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        border.width: 2
        border.color: Kirigami.Theme.highlightColor

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            icon.source: "qrc:/icons/app.svg"
            text: i18n("Let go to install this app")
        }
    }

    onClosing: close => {
        if (Installer.busy) {
            close.accepted = false;
            showPassiveNotification(i18n("Please wait until the installation is finished."));
        }
    }

    // Settings and About are pages on top of the main one. Neither can be
    // opened while installing, and an install can't start from them.
    function openSettings() {
        if (Installer.busy || pageStack.layers.currentItem?.objectName === "settingsPage") {
            return;
        }
        // From the about page, go back to the settings underneath it.
        if (pageStack.layers.depth > 1) {
            pageStack.layers.pop();
        }
        if (pageStack.layers.currentItem?.objectName !== "settingsPage") {
            pageStack.layers.push(settingsPage);
        }
    }

    function openAbout() {
        if (!Installer.busy && pageStack.layers.currentItem?.objectName !== "aboutPage") {
            pageStack.layers.push(aboutPage);
        }
    }

    function chooseFile() {
        if (!Installer.busy) {
            pageStack.layers.clear();
            fileDialog.open();
        }
    }

    Connections {
        target: Installer
        function onStateChanged() {
            if (Installer.busy) {
                root.pageStack.layers.clear();
            }
        }
    }

    // The menu bar in the global menu (KDE Plasma). Without one, nothing is
    // shown in the window and the gear button is the way to the settings.
    Platform.MenuBar {
        window: root

        Platform.Menu {
            title: i18nc("Menu bar", "File")

            Platform.MenuItem {
                text: i18n("Choose File…")
                shortcut: StandardKey.Open
                enabled: !Installer.busy
                onTriggered: root.chooseFile()
            }
            Platform.MenuItem {
                text: i18n("Quit")
                shortcut: StandardKey.Quit
                onTriggered: root.close()
            }
        }

        Platform.Menu {
            title: i18nc("Menu bar", "Settings")

            Platform.MenuItem {
                text: i18n("Settings…")
                shortcut: "Ctrl+,"
                enabled: !Installer.busy
                onTriggered: root.openSettings()
            }
        }

        Platform.Menu {
            title: i18nc("Menu bar", "Help")

            Platform.MenuItem {
                text: i18n("About")
                enabled: !Installer.busy
                onTriggered: root.openAbout()
            }
        }
    }

    Component {
        id: settingsPage
        SettingsPage {
            objectName: "settingsPage"
            onAboutRequested: root.openAbout()
        }
    }

    Component {
        id: aboutPage
        FormCard.AboutPage {
            objectName: "aboutPage"
        }
    }

    Kirigami.PromptDialog {
        id: restartDialog
        // Written in the language that was chosen; see Preferences.
        title: Preferences.restartTitle
        subtitle: Preferences.restartExplanation
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: Preferences.restartNowText
                icon.name: "view-refresh"
                onTriggered: {
                    restartDialog.close();
                    if (!Preferences.restart()) {
                        root.showPassiveNotification(i18n("Couldn't restart the app. Please close it and open it again."));
                    }
                }
            },
            Kirigami.Action {
                text: Preferences.restartLaterText
                onTriggered: restartDialog.close()
            }
        ]
    }

    Connections {
        target: Preferences
        function onRestartPrompted() {
            restartDialog.open();
        }
    }

    FileDialog {
        id: fileDialog
        nameFilters: [i18nc("File dialog filter; keep the parentheses and the patterns unchanged", "Flatpak apps (*.flatpak *.flatpakref)")]
        onAccepted: Installer.open(selectedFile)
    }

    Component {
        id: emptyView
        Kirigami.PlaceholderMessage {
            icon.source: "qrc:/icons/app.svg"
            text: i18n("Install an app")
            explanation: i18n("Double-click an app you downloaded (a file ending in “.flatpak” or “.flatpakref”), drag it onto this window, or choose one here.")
            helpfulAction: Kirigami.Action {
                text: i18n("Choose File…")
                onTriggered: fileDialog.open()
            }
        }
    }

    Component {
        id: problemView
        Kirigami.PlaceholderMessage {
            icon.source: "qrc:/icons/error.svg"
            text: Installer.errorMessage
            explanation: Installer.state === Installer.State.FlatpakMissing ? i18n("Ask the person who manages this computer to install Flatpak, or see flatpak.org/setup for instructions.") : ""
            helpfulAction: Kirigami.Action {
                enabled: Installer.state === Installer.State.Invalid
                text: i18n("Choose Another File…")
                onTriggered: fileDialog.open()
            }
        }
    }

    Component {
        id: checkingView
        Kirigami.LoadingPlaceholder {
            text: i18n("Getting ready…")
        }
    }

    Component {
        id: readyView
        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Heading {
                Layout.fillWidth: true
                text: i18n("Install “%1”", Installer.appName)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: {
                    const parts = [];
                    if (Installer.appSummary)
                        parts.push(Installer.appSummary);
                    if (Installer.appVersion)
                        parts.push(i18n("Version %1", Installer.appVersion));
                    if (Installer.appDeveloper)
                        parts.push(i18n("By %1", Installer.appDeveloper));
                    if (Installer.appSize)
                        parts.push(i18n("Needs %1 of space", Installer.appSize));
                    return parts.join(" · ");
                }
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                opacity: 0.7
            }

            PermissionsCard {
                Layout.fillWidth: true
                Layout.maximumHeight: Kirigami.Units.gridUnit * 18
                Layout.preferredHeight: implicitHeight
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Kirigami.Units.gridUnit * 2

                AppTile {
                    Layout.alignment: Qt.AlignVCenter
                    name: Installer.appName
                    iconSource: Installer.appIcon
                }

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Kirigami.Units.iconSizes.large
                    implicitHeight: Kirigami.Units.iconSizes.large
                    source: "qrc:/icons/arrow.svg"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: Kirigami.Units.largeSpacing

                    DropTarget {
                        Layout.fillWidth: true
                        title: i18n("Just for me")
                        description: i18n("Only you can use it. No password needed.")
                        iconSource: "qrc:/icons/user.svg"
                        onChosen: Installer.install(false)
                    }

                    DropTarget {
                        Layout.fillWidth: true
                        visible: Installer.systemAvailable
                        title: i18n("Everyone on this computer")
                        description: i18n("Everyone who uses this computer can open it. You may need to enter your password.")
                        iconSource: "qrc:/icons/computer.svg"
                        onChosen: Installer.install(true)
                    }
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: i18n("Drag the app onto where you want to install it.")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }
    }

    Component {
        id: existingView
        ColumnLayout {
            id: existing

            readonly property bool changeIsMain: Installer.existing === Installer.Existing.FileIsNewer || Installer.existing === Installer.Existing.UpdateAvailable
            readonly property bool showsPermissions: Installer.offersChange && Installer.existing !== Installer.Existing.SameVersion

            spacing: Kirigami.Units.largeSpacing

            Item {
                Layout.fillHeight: true
            }

            Kirigami.Icon {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: Kirigami.Units.iconSizes.huge
                implicitHeight: Kirigami.Units.iconSizes.huge
                source: Installer.appIcon
            }

            Kirigami.Heading {
                Layout.fillWidth: true
                text: i18n("“%1” is already on this computer", Installer.appName)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            QQC2.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: {
                    const where = Installer.existingForEveryone ? i18n("It's installed for everyone on this computer.") : i18n("It's installed just for you.");
                    let what = "";
                    switch (Installer.existing) {
                    case Installer.Existing.SameVersion:
                        what = Installer.existingVersion ? i18n("You already have this same version (%1).", Installer.existingVersion) : i18n("You already have this same version.");
                        break;
                    case Installer.Existing.FileIsNewer:
                        what = i18n("This file has a newer version (%1) than the one you have (%2).", Installer.appVersion, Installer.existingVersion);
                        break;
                    case Installer.Existing.FileIsOlder:
                        what = i18n("You already have a newer version (%1). This file has an older one (%2).", Installer.existingVersion, Installer.appVersion);
                        break;
                    case Installer.Existing.DifferentVersion:
                        what = i18n("This file has a different version from the one you have.");
                        break;
                    case Installer.Existing.UpdateAvailable:
                        what = i18n("A newer version is available.");
                        break;
                    case Installer.Existing.UpToDate:
                        what = i18n("You already have the newest version.");
                        break;
                    case Installer.Existing.Unknown:
                        what = i18n("You can check whether there's a newer version.");
                        break;
                    }
                    return where + " " + what;
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                opacity: 0.7
                visible: Installer.offersChange || !Installer.canChangeExisting
                text: Installer.canChangeExisting ? i18n("Your settings and files in the app will be kept.") : i18n("Only someone who manages this computer can change it.")
            }

            PermissionsCard {
                Layout.fillWidth: true
                Layout.maximumHeight: Kirigami.Units.gridUnit * 14
                Layout.preferredHeight: implicitHeight
                visible: existing.showsPermissions
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: Kirigami.Units.largeSpacing

                QQC2.Button {
                    visible: Installer.offersChange && existing.changeIsMain
                    highlighted: true
                    text: Installer.existing === Installer.Existing.UpdateAvailable ? i18n("Update") : i18n("Update to This Version")
                    onClicked: Installer.changeExisting()
                }
                QQC2.Button {
                    visible: Installer.canLaunch
                    highlighted: !existing.changeIsMain || !Installer.offersChange
                    text: Installer.existing === Installer.Existing.FileIsNewer || Installer.existing === Installer.Existing.FileIsOlder || Installer.existing === Installer.Existing.DifferentVersion ? i18n("Open the Current Version") : i18n("Open “%1”", Installer.appName)
                    onClicked: {
                        Installer.openExisting();
                        Qt.quit();
                    }
                }
                QQC2.Button {
                    visible: Installer.offersChange && !existing.changeIsMain
                    text: {
                        switch (Installer.existing) {
                        case Installer.Existing.SameVersion:
                            return i18n("Install Again");
                        case Installer.Existing.FileIsOlder:
                            return i18n("Switch to This Older Version");
                        case Installer.Existing.DifferentVersion:
                            return i18n("Switch to This Version");
                        }
                        return i18n("Check for Updates");
                    }
                    onClicked: Installer.changeExisting()
                }
                QQC2.Button {
                    text: i18n("Close")
                    onClicked: Qt.quit()
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }

    Component {
        id: installingView
        Kirigami.LoadingPlaceholder {
            text: Installer.outcome === Installer.Outcome.Installed ? i18n("Installing “%1”…", Installer.appName) : i18n("Updating “%1”…", Installer.appName)
            explanation: Installer.downloadsExtras ? i18n("This app needs some extra parts from the internet, so this may take a while. Please keep this window open.") : i18n("This may take a few minutes. Please keep this window open.")
        }
    }

    Component {
        id: installedView
        Item {
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width

                Kirigami.PlaceholderMessage {
                    Layout.fillWidth: true
                    icon.source: "qrc:/icons/done.svg"
                    text: {
                        switch (Installer.outcome) {
                        case Installer.Outcome.Updated:
                            return i18n("“%1” is updated", Installer.appName);
                        case Installer.Outcome.UpToDate:
                            return i18n("“%1” is up to date", Installer.appName);
                        }
                        return i18n("“%1” is installed", Installer.appName);
                    }
                    explanation: Installer.installedForEveryone ? i18n("Everyone on this computer can now find it in the app menu.") : i18n("You can now find it in your app menu.")
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    QQC2.Button {
                        visible: Installer.canLaunch
                        text: i18n("Open “%1”", Installer.appName)
                        onClicked: {
                            Installer.launch();
                            Qt.quit();
                        }
                    }
                    QQC2.Button {
                        text: i18n("Done")
                        onClicked: Qt.quit()
                    }
                }
            }
        }
    }

    Component {
        id: failedView
        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            Item {
                Layout.fillHeight: true
            }

            Kirigami.PlaceholderMessage {
                Layout.fillWidth: true
                icon.source: "qrc:/icons/error.svg"
                text: Installer.errorMessage
                explanation: i18n("Please check your internet connection and try again.")
            }

            QQC2.ScrollView {
                id: details
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 8
                visible: false

                QQC2.TextArea {
                    text: Installer.errorDetails
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    font.family: "monospace"
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                QQC2.Button {
                    visible: Installer.errorDetails.length > 0
                    text: details.visible ? i18n("Hide Details") : i18n("Show Details")
                    onClicked: details.visible = !details.visible
                }
                QQC2.Button {
                    text: i18n("Try Again")
                    onClicked: Installer.tryAgain()
                }
                QQC2.Button {
                    text: i18n("Close")
                    onClicked: Qt.quit()
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
