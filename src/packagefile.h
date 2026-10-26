// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "packageinfo.h"

#include <QByteArray>
#include <QHash>
#include <QStringList>

#include <optional>

// Reads .flatpak bundles and .flatpakref files without installing them.
// Only reads the file; never touches any Flatpak installation.
namespace PackageFile
{

enum class Error {
    None,
    Unreadable, // file missing or no permission
    NotFlatpak, // not a bundle or ref we understand
};

struct Result {
    std::optional<PackageInfo> info;
    Error error = Error::None;
};

// `languages` are preferred UI languages, most preferred first, in
// gettext form (e.g. "zh_TW", "zh"). Used to pick a localized app name.
Result load(const QString &path, const QStringList &languages);

Result parseBundle(const QByteArray &data, const QStringList &languages);
Result parseRef(const QByteArray &data);

// Whether a file name looks like something we can install, judging only by
// its extension: ".flatpak" or ".flatpakref", in any letter case.
bool isInstallableFileName(const QString &fileName);

// Picks the value for the first matching language. Keys are language tags
// as found in AppStream ("zh_TW", "zh-Hant-TW", ...); "" is the default.
QString localizedValue(const QHash<QString, QString> &values, const QStringList &languages);

} // namespace PackageFile
