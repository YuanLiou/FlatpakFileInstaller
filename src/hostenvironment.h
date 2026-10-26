// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "packageinfo.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// Facts about the host system that decide which install targets we offer.
// The functions here are pure so they can be unit tested; Installer runs
// the actual host commands and feeds their output in.
namespace HostEnvironment
{

struct OsRelease {
    QString id;
    QStringList idLike;
    QString prettyName;
};

OsRelease parseOsRelease(const QByteArray &contents);

// The host's os-release. Inside Flatpak, /etc/os-release describes the
// runtime, so the host's copy at /run/host/os-release is read instead.
OsRelease readHostOsRelease(bool inFlatpak);

bool isSteamOS(const OsRelease &os);

// Result of `pkcheck --action-id <action> --process <pid>`.
enum class Authorization {
    Allowed, // no password needed
    Denied, // not allowed at all
    NeedsPassword, // allowed after admin authentication
    Unknown, // pkcheck missing or failed
};

Authorization authorizationFromPkcheck(int exitCode, bool ranNormally);

// polkit actions flatpak's system helper checks for this install.
QStringList systemInstallActions(const PackageInfo &package, bool remoteAlreadyConfigured);

// Whether `remoteUrl` is one of the configured remotes' URLs
// (output of `flatpak remotes --system --columns=url`).
bool isRemoteConfigured(const QString &remoteUrl, const QByteArray &remotesOutput);

// Whether to show the "System" drop target.
bool offerSystemInstall(bool hasSystemInstallation, const QList<Authorization> &authorizations, bool steamOS);

} // namespace HostEnvironment
