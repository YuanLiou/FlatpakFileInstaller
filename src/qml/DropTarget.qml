// SPDX-License-Identifier: Apache-2.0
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A place the app can be dropped on, like the Applications folder in a
// macOS disk image.
DropArea {
    id: target

    readonly property string dragKey: "application/x-flatpak-file-installer"

    required property string title
    required property string description
    required property url iconSource

    signal chosen

    keys: [dragKey]
    implicitWidth: card.implicitWidth
    implicitHeight: card.implicitHeight

    // Installing replaces this view; let the drag finish first.
    onDropped: drop => {
        drop.accept();
        Qt.callLater(target.chosen);
    }

    Rectangle {
        id: card

        anchors.fill: parent
        implicitWidth: row.implicitWidth + Kirigami.Units.largeSpacing * 2
        implicitHeight: row.implicitHeight + Kirigami.Units.largeSpacing * 2
        radius: Kirigami.Units.cornerRadius
        color: target.containsDrag ? Qt.alpha(Kirigami.Theme.highlightColor, 0.2) : Kirigami.Theme.alternateBackgroundColor
        border.width: target.containsDrag ? 2 : 1
        border.color: target.containsDrag ? Kirigami.Theme.highlightColor : Qt.alpha(Kirigami.Theme.textColor, 0.2)

        Behavior on color {
            ColorAnimation {
                duration: Kirigami.Units.shortDuration
            }
        }

        RowLayout {
            id: row

            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                implicitWidth: Kirigami.Units.iconSizes.large
                implicitHeight: Kirigami.Units.iconSizes.large
                source: target.iconSource
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 3
                    text: target.title
                    wrapMode: Text.Wrap
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: target.description
                    wrapMode: Text.Wrap
                    opacity: 0.7
                }
            }
        }
    }
}
