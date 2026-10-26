// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "packageinfo.h"

#include <QStringList>

// Arguments for the host's `flatpak` command.
namespace FlatpakCli
{

// Flathub's repository, as `flatpak remotes --columns=url` prints it.
QString flathubUrl();
// The .flatpakrepo file that sets Flathub up.
QString flathubRepoFile();
QString flathubRemoteName();

// Installs the bundle or ref at `path` for the current user or system-wide.
QStringList installArguments(const PackageInfo &package, const QString &path, bool forEveryone);

// Starts an installed app.
QStringList runArguments(const PackageInfo &package, bool forEveryone);

// Adds a remote from a .flatpakrepo file, unless one with that name exists.
QStringList addRemoteArguments(const QString &name, const QString &repoFile, bool forEveryone);

// Lists the configured remotes as "name<TAB>url" lines.
QStringList remotesArguments(bool forEveryone);

// Prints the metadata (including permissions) of `package` in `remote`.
QStringList remoteInfoArguments(const QString &remote, const PackageInfo &package, bool forEveryone);

// Succeeds when `runtime` (e.g. "org.kde.Platform/x86_64/6.10") is installed.
QStringList runtimeInfoArguments(const QString &runtime, bool forEveryone);

} // namespace FlatpakCli
