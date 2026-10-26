// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.yuanliou.FlatpakFileInstaller

// Where the app comes from and what it may access, shown before installing.
// Things worth a second thought get a small orange mark, never red.
Rectangle {
    id: card

    readonly property bool anyPermissionNeedsTrust: Installer.permissions.some(permission => permission.needsTrust)

    implicitHeight: content.implicitHeight + scrollView.topPadding + scrollView.bottomPadding + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Kirigami.Theme.alternateBackgroundColor
    border.width: 1
    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.15)

    QQC2.ScrollView {
        id: scrollView

        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        contentWidth: availableWidth

        ColumnLayout {
            id: content

            width: parent.width
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                    source: Installer.sourceNeedsTrust ? "qrc:/icons/error.svg" : "qrc:/icons/info.svg"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: Installer.sourceTitle
                        font.bold: true
                        wrapMode: Text.Wrap
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: Installer.sourceExplanation
                        wrapMode: Text.Wrap
                        opacity: 0.8
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: {
                    if (!Installer.permissionsKnown)
                        return i18n("We couldn't find out what this app can access.");
                    if (Installer.permissions.length === 0)
                        return i18n("This app doesn't need any special access.");
                    return i18n("This app can:");
                }
                font.bold: true
                wrapMode: Text.Wrap
            }

            Repeater {
                model: Installer.permissions

                delegate: RowLayout {
                    id: permissionRow

                    required property var modelData

                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.Icon {
                        Layout.alignment: Qt.AlignTop
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                        source: permissionRow.modelData.needsTrust ? "qrc:/icons/error.svg" : "qrc:/icons/dot.svg"
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: permissionRow.modelData.text
                        wrapMode: Text.Wrap
                    }
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                visible: card.anyPermissionNeedsTrust
                text: i18n("Many good apps need this kind of access. Just make sure you trust this one.")
                wrapMode: Text.Wrap
                opacity: 0.8
            }
        }
    }
}
