// SPDX-License-Identifier: Apache-2.0
import QtQuick
import org.kde.coreaddons as Core
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
        FormCard.FormComboBoxDelegate {
            id: languageBox
            text: i18n("Language")
            description: i18n("Used the next time you open this app.")
            model: Preferences.languages
            textRole: "name"
            valueRole: "code"
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Preferences.language))
            onActivated: Preferences.chooseLanguage(currentValue)
        }
    }

    FormCard.FormHeader {
        title: i18n("About")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: i18n("Version")
            description: Core.AboutData.version
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: i18n("License")
            description: Core.AboutData.licenses.length > 0 ? Core.AboutData.licenses[0].name : ""
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormLinkDelegate {
            text: i18n("Project page")
            url: Core.AboutData.homepage
            visible: Core.AboutData.homepage.length > 0
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: i18n("About Flatpak File Installer")
            onClicked: page.aboutRequested()
        }
    }
}
