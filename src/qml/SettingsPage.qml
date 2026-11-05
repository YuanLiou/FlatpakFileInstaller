// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.coreaddons as Core
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import io.github.yuanliou.FlatpakFileInstaller

FormCard.FormCardPage {
    id: page

    signal aboutRequested

    title: i18n("Settings")

    FormCard.FormHeader {
        title: i18n("Language")
    }

    FormCard.FormCard {
        // Not a FormComboBoxDelegate: that makes the whole row a button, so a
        // click on the label or the empty space around it opens the list. Here
        // only the combo box does.
        ColumnLayout {
            Layout.fillWidth: true
            // As far in as the text of the other rows.
            Layout.margins: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                Layout.fillWidth: true
                text: i18n("Language")
                wrapMode: Text.Wrap
            }

            QQC2.ComboBox {
                id: languageBox
                Layout.fillWidth: true
                model: Preferences.languages
                textRole: "name"
                valueRole: "code"
                Accessible.name: i18n("Language")
                Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Preferences.language))
                onActivated: Preferences.chooseLanguage(currentValue)

                // The popup of org.kde.desktop's ComboBox is only as wide as its
                // widest item in newer Plasma versions (the runtime's), not as wide
                // as the combo box. Match the combo box, or its widest item if that
                // is wider, but never wider than the window.
                Binding {
                    target: languageBox.popup
                    property: "width"
                    value: Math.min(Math.max(languageBox.width, languageBox.popup.implicitWidth), QQC2.Overlay.overlay ? QQC2.Overlay.overlay.width : languageBox.width)
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: i18n("Used the next time you open this app.")
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
            }
        }
    }

    FormCard.FormHeader {
        title: i18n("About")
    }

    // Each separator names its neighbours. Left to itself it looks for them in
    // the focus chain, which skips the text delegates and so ends up hiding
    // lines far away when a link is hovered.
    FormCard.FormCard {
        FormCard.FormTextDelegate {
            id: versionDelegate
            text: i18n("Version")
            description: Core.AboutData.version
        }

        FormCard.FormDelegateSeparator {
            above: versionDelegate
            below: licenseDelegate
        }

        FormCard.FormTextDelegate {
            id: licenseDelegate
            text: i18n("License")
            description: Core.AboutData.licenses.length > 0 ? Core.AboutData.licenses[0].name : ""
        }

        FormCard.FormDelegateSeparator {
            above: licenseDelegate
            below: projectDelegate
        }

        FormCard.FormLinkDelegate {
            id: projectDelegate
            text: i18n("Project page")
            url: Core.AboutData.homepage
            visible: Core.AboutData.homepage.length > 0
        }

        FormCard.FormDelegateSeparator {
            above: projectDelegate
            below: aboutDelegate
        }

        FormCard.FormButtonDelegate {
            id: aboutDelegate
            text: i18n("About Flatpak File Installer")
            onClicked: page.aboutRequested()
        }
    }
}
