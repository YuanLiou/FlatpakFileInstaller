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

// What one installation (the user's or the system's) already has.
struct InstallationState {
    bool runtimeInstalled = true; // the app's runtime can be used from it
    bool flathubConfigured = false;
    bool remoteConfigured = false; // the .flatpakref's own remote
    QString remoteName; // its name, when configured
};

// Whether to add Flathub before installing, so that flatpak can download
// a runtime the bundle needs. Bundles with a runtime-repo URL don't need
// this: flatpak adds that repository by itself.
bool needsFlathub(const PackageInfo &package, const InstallationState &state);

// polkit actions flatpak's system helper checks for this install.
QStringList systemInstallActions(const PackageInfo &package, const InstallationState &system);

// polkit action for updating a system-wide app or runtime from its remote.
// Allowed without a password by flatpak's default policy.
QStringList systemUpdateActions(const PackageInfo &package);

// The name of the configured remote whose URL is `remoteUrl`, or an empty
// string. `remotesOutput` is the output of
// `flatpak remotes --columns=name,url`: one "name<TAB>url" line per remote.
QString remoteNameForUrl(const QString &remoteUrl, const QByteArray &remotesOutput);

// Whether to show the "System" drop target.
bool offerSystemInstall(bool hasSystemInstallation, const QList<Authorization> &authorizations, bool steamOS);

} // namespace HostEnvironment
