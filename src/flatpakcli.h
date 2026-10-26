// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "packageinfo.h"

#include <QStringList>

// Arguments for the host's `flatpak` command.
namespace FlatpakCli
{

// Installs the bundle or ref at `path` for the current user or system-wide.
QStringList installArguments(const PackageInfo &package, const QString &path, bool forEveryone);

// Starts an installed app.
QStringList runArguments(const PackageInfo &package, bool forEveryone);

} // namespace FlatpakCli
