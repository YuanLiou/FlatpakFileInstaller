// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.yuanliou.FlatpakAppInstaller

Kirigami.ApplicationWindow {
    id: root

    title: Installer.state === Installer.Empty ? i18n("Flatpak App Installer") : i18n("Install “%1”", Installer.appName)
    width: Kirigami.Units.gridUnit * 38
    height: Kirigami.Units.gridUnit * 24
    minimumWidth: Kirigami.Units.gridUnit * 30
    minimumHeight: Kirigami.Units.gridUnit * 20

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    pageStack.initialPage: Kirigami.Page {
        Loader {
            anchors.fill: parent
            sourceComponent: {
                switch (Installer.state) {
                case Installer.Empty:
                    return emptyView;
                case Installer.Invalid:
                case Installer.FlatpakMissing:
                    return problemView;
                case Installer.Checking:
                    return checkingView;
                case Installer.Ready:
                    return readyView;
                case Installer.Installing:
                    return installingView;
                case Installer.Installed:
                    return installedView;
                case Installer.Failed:
                    return failedView;
                }
                return emptyView;
            }
        }
    }

    onClosing: close => {
        if (Installer.busy) {
            close.accepted = false;
            showPassiveNotification(i18n("Please wait until the installation is finished."));
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
            explanation: i18n("Double-click an app you downloaded (a file ending in “.flatpak” or “.flatpakref”), or choose one here.")
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
            explanation: Installer.state === Installer.FlatpakMissing ? i18n("Ask the person who manages this computer to install Flatpak, or see flatpak.org/setup for instructions.") : ""
            helpfulAction: Kirigami.Action {
                enabled: Installer.state === Installer.Invalid
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
        id: installingView
        Kirigami.LoadingPlaceholder {
            text: i18n("Installing “%1”…", Installer.appName)
            explanation: i18n("This may take a few minutes. Please keep this window open.")
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
                    text: i18n("“%1” is installed", Installer.appName)
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
