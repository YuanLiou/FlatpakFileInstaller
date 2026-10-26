// SPDX-License-Identifier: Apache-2.0
#include "permissiontext.h"

#include <KLocalizedString>

#include <QUrl>

using namespace Qt::StringLiterals;
using Permissions::Kind;

namespace PermissionText
{

namespace
{

QString folderName(const QString &folder)
{
    const QString base = folder.section(u'/', 0, 0);
    const QString rest = folder.section(u'/', 1);
    QString name;
    if (base == "xdg-desktop"_L1) {
        name = i18nc("folder name", "Desktop");
    } else if (base == "xdg-documents"_L1) {
        name = i18nc("folder name", "Documents");
    } else if (base == "xdg-download"_L1) {
        name = i18nc("folder name", "Downloads");
    } else if (base == "xdg-music"_L1) {
        name = i18nc("folder name", "Music");
    } else if (base == "xdg-pictures"_L1) {
        name = i18nc("folder name", "Pictures");
    } else if (base == "xdg-public-share"_L1) {
        name = i18nc("folder name", "Public");
    } else if (base == "xdg-templates"_L1) {
        name = i18nc("folder name", "Templates");
    } else if (base == "xdg-videos"_L1) {
        name = i18nc("folder name", "Videos");
    } else {
        return i18nc("a folder in a list of folders; %1 is its path", "“%1”", folder);
    }
    return i18nc("a folder in a list of folders; %1 is its name", "“%1”", rest.isEmpty() ? name : name + u'/' + rest);
}

QString folderList(const QStringList &folders)
{
    QStringList names;
    for (const QString &folder : folders) {
        names.append(folderName(folder));
    }
    return names.join(i18nc("separator between folder names in a list", ", "));
}

} // namespace

QString describe(const Permissions::Permission &permission)
{
    switch (permission.kind) {
    case Kind::RunAnyProgram:
        return i18n("Run any program on this computer");
    case Kind::AllFiles:
        return permission.readOnly ? i18n("See all files on this computer") : i18n("See and change all files on this computer");
    case Kind::HomeFolder:
        return permission.readOnly ? i18n("See all your personal files") : i18n("See and change all your personal files");
    case Kind::SystemFiles:
        return permission.readOnly ? i18n("See system files and settings") : i18n("Change system files and settings");
    case Kind::AllDevices:
        return i18n("Use all devices, such as cameras, microphones and USB drives");
    case Kind::Passwords:
        return i18n("Read passwords saved on this computer");
    case Kind::SshKeys:
        return i18n("Use your SSH keys for signing in to servers");
    case Kind::EncryptionKeys:
        return i18n("Use your encryption keys");
    case Kind::OtherAppsData:
        return i18n("Change the settings and data of other apps");
    case Kind::AllAppsBus:
        return i18n("Talk freely with other programs on this computer");
    case Kind::SystemBus:
        return i18n("Talk freely with the system's services");
    case Kind::LegacyDisplay:
        return i18n("Use the older display system, which can let it see what you type in other windows");
    case Kind::Folders:
        if (permission.folders.size() == 1) {
            const QString folder = folderName(permission.folders.first());
            return permission.readOnly ? i18n("See files in the %1 folder", folder) : i18n("See and change files in the %1 folder", folder);
        }
        return permission.readOnly ? i18n("See files in these folders: %1", folderList(permission.folders))
                                   : i18n("See and change files in these folders: %1", folderList(permission.folders));
    case Kind::Network:
        return i18n("Use the internet");
    case Kind::Sound:
        return i18n("Play sound and use the microphone");
    case Kind::Printing:
        return i18n("Use printers");
    case Kind::Bluetooth:
        return i18n("Use Bluetooth");
    case Kind::SmartCards:
        return i18n("Use smart card readers");
    case Kind::GameControllers:
        return i18n("Use game controllers and other input devices");
    case Kind::UsbDevices:
        return i18n("Use USB devices");
    case Kind::Virtualization:
        return i18n("Run virtual machines");
    }
    return {};
}

QString sourceTitle(const PackageInfo &package)
{
    switch (Permissions::sourceOf(package)) {
    case Permissions::Source::File:
        return i18n("A file you downloaded");
    case Permissions::Source::Flathub:
        return i18n("From Flathub");
    case Permissions::Source::OtherSite: {
        const QString host = QUrl(package.remoteUrl).host();
        return host.isEmpty() ? i18n("From another source") : i18n("From %1", host);
    }
    }
    return {};
}

QString sourceExplanation(const PackageInfo &package)
{
    switch (Permissions::sourceOf(package)) {
    case Permissions::Source::File:
        return i18n("It doesn't come from an app store, so there's no way to check who made it. Install it only if you trust the website you got it from.");
    case Permissions::Source::Flathub:
        return i18n("Flathub is the biggest app store for Linux.");
    case Permissions::Source::OtherSite:
        return i18n("This isn't a well-known app store. Install it only if you trust this website.");
    }
    return {};
}

bool sourceNeedsTrust(const PackageInfo &package)
{
    return Permissions::sourceOf(package) != Permissions::Source::Flathub;
}

} // namespace PermissionText
