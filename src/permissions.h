// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "packageinfo.h"

#include <QByteArray>
#include <QList>
#include <QStringList>

// What an app may access, read from its Flatpak metadata ([Context] and
// [Session Bus Policy]). Only access worth telling a non-technical person
// about is reported; graphics, IPC and similar plumbing is left out.
namespace Permissions
{

// In the order they should be shown: the ones to think about first.
enum class Kind {
    RunAnyProgram, // talks to org.freedesktop.Flatpak, so can leave its sandbox
    AllFiles,
    HomeFolder,
    SystemFiles,
    AllDevices,
    Passwords,
    SshKeys,
    EncryptionKeys,
    OtherAppsData,
    AllAppsBus,
    SystemBus,
    LegacyDisplay,
    Folders,
    Network,
    Sound,
    Printing,
    Bluetooth,
    SmartCards,
    GameControllers,
    UsbDevices,
    Virtualization,
};

struct Permission {
    Kind kind;
    bool readOnly = false; // for file access
    // For Folders: as written in the metadata, e.g. "xdg-download",
    // "xdg-pictures/Screenshots", "~/Games" or "/mnt/data".
    QStringList folders;

    bool operator==(const Permission &) const = default;
};

// Whether the permission deserves a gentle "make sure you trust it" note.
bool needsTrust(Kind kind);

QList<Permission> fromMetadata(const QByteArray &metadata);

enum class Source {
    File, // a .flatpak bundle: nobody vouches for where it came from
    Flathub,
    OtherSite, // a .flatpakref pointing somewhere other than Flathub
};

Source sourceOf(const PackageInfo &package);

} // namespace Permissions
