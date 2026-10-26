// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The app's icon and name, which the user drags onto a DropTarget.
// It slides back into place when released anywhere else.
Item {
    id: tile

    required property string name
    required property var iconSource

    implicitWidth: Kirigami.Units.gridUnit * 9
    implicitHeight: content.implicitHeight
    z: mouseArea.drag.active ? 1 : 0

    Item {
        id: dragItem

        width: tile.width
        height: tile.height
        scale: mouseArea.drag.active ? 1.08 : 1

        Drag.active: mouseArea.drag.active
        Drag.keys: ["application/x-flatpak-file-installer"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: Kirigami.Units.iconSizes.enormous / 2

        Behavior on x {
            enabled: !mouseArea.drag.active
            NumberAnimation {
                duration: Kirigami.Units.longDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on y {
            enabled: !mouseArea.drag.active
            NumberAnimation {
                duration: Kirigami.Units.longDuration
                easing.type: Easing.OutCubic
            }
        }
        Behavior on scale {
            NumberAnimation {
                duration: Kirigami.Units.shortDuration
            }
        }

        ColumnLayout {
            id: content
            width: parent.width
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: Kirigami.Units.iconSizes.enormous
                implicitHeight: Kirigami.Units.iconSizes.enormous
                source: tile.iconSource
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: tile.name
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            drag.target: dragItem
            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor

            onReleased: {
                if (dragItem.Drag.target) {
                    dragItem.Drag.drop();
                }
                dragItem.x = 0;
                dragItem.y = 0;
            }
        }
    }
}
