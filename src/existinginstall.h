// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "packageinfo.h"

#include <QByteArray>
#include <QString>

#include <optional>

// Finding out whether the app in the file is already installed, and how
// the installed copy relates to the one in the file.
namespace ExistingInstall
{

struct Installed {
    bool forEveryone = false; // system-wide rather than for this user
    QString branch;
    QString version; // from AppStream; may be empty
    QString commit; // usually the 12-character prefix `flatpak list` shows
};

// Finds `package` in the output of
// `flatpak list --columns=application,branch,arch,version,active`
// (one tab-separated line per installed app or runtime).
std::optional<Installed> findInList(const PackageInfo &package, const QByteArray &listOutput, bool forEveryone);

enum class Comparison {
    SameVersion,
    FileIsNewer,
    FileIsOlder,
    DifferentVersion, // which one is newer can't be told
    UpdateAvailable, // .flatpakref: the remote has a newer build
    UpToDate, // .flatpakref: the installed build is the remote's newest
    Unknown, // .flatpakref whose remote couldn't be asked
};

// `latestCommit` is the remote's current commit for a .flatpakref, when
// known; bundles carry their own commit.
Comparison compare(const PackageInfo &package, const Installed &installed, const QString &latestCommit = {});

// Compares version strings the way package managers do: digit runs as
// numbers, letter runs as text. Returns <0, 0 or >0.
int compareVersions(const QString &a, const QString &b);

} // namespace ExistingInstall
