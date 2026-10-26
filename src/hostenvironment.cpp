// SPDX-License-Identifier: Apache-2.0
#include "hostenvironment.h"

#include <QFile>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace HostEnvironment
{

namespace
{

QString unquote(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && (value.front() == u'"' || value.front() == u'\'') && value.back() == value.front()) {
        value = value.mid(1, value.size() - 2);
        value.replace(u"\\\""_s, u"\""_s).replace(u"\\\\"_s, u"\\"_s);
    }
    return value;
}

QString normalizedUrl(const QString &url)
{
    QUrl normalized = QUrl(url.trimmed()).adjusted(QUrl::NormalizePathSegments | QUrl::StripTrailingSlash);
    // StripTrailingSlash keeps a lone "/", so "https://host/" and "https://host" would differ.
    if (normalized.path() == "/"_L1) {
        normalized.setPath(QString());
    }
    return normalized.toString().toLower();
}

} // namespace

OsRelease parseOsRelease(const QByteArray &contents)
{
    OsRelease os;
    const QStringList lines = QString::fromUtf8(contents).split(u'\n');
    for (const QString &line : lines) {
        const qsizetype eq = line.indexOf(u'=');
        if (line.startsWith(u'#') || eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        const QString value = unquote(line.mid(eq + 1));
        if (key == "ID"_L1) {
            os.id = value.toLower();
        } else if (key == "ID_LIKE"_L1) {
            os.idLike = value.toLower().split(u' ', Qt::SkipEmptyParts);
        } else if (key == "PRETTY_NAME"_L1) {
            os.prettyName = value;
        }
    }
    return os;
}

OsRelease readHostOsRelease(bool inFlatpak)
{
    const QStringList candidates = inFlatpak ? QStringList{u"/run/host/os-release"_s} : QStringList{u"/etc/os-release"_s, u"/usr/lib/os-release"_s};
    for (const QString &path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            return parseOsRelease(file.readAll());
        }
    }
    return {};
}

bool isSteamOS(const OsRelease &os)
{
    return os.id == "steamos"_L1;
}

Authorization authorizationFromPkcheck(int exitCode, bool ranNormally)
{
    if (!ranNormally) {
        return Authorization::Unknown;
    }
    switch (exitCode) {
    case 0:
        return Authorization::Allowed;
    case 1:
        return Authorization::Denied;
    case 2: // authentication required; we never pass --allow-user-interaction
        return Authorization::NeedsPassword;
    default:
        return Authorization::Unknown;
    }
}

bool needsFlathub(const PackageInfo &package, const InstallationState &state)
{
    return package.format == PackageInfo::Format::Bundle && package.kind == PackageInfo::Kind::App && !package.runtime.isEmpty()
        && package.runtimeRepoUrl.isEmpty() && !state.runtimeInstalled && !state.flathubConfigured;
}

QStringList systemInstallActions(const PackageInfo &package, const InstallationState &system)
{
    QStringList actions;
    if (package.format == PackageInfo::Format::Bundle) {
        actions.append(u"org.freedesktop.Flatpak.install-bundle"_s);
        if (!system.runtimeInstalled) {
            actions.append(u"org.freedesktop.Flatpak.runtime-install"_s);
            if (!system.flathubConfigured) {
                actions.append(u"org.freedesktop.Flatpak.configure-remote"_s);
            }
        }
        return actions;
    }
    if (!system.remoteConfigured) {
        actions.append(u"org.freedesktop.Flatpak.configure-remote"_s);
    }
    actions.append(package.kind == PackageInfo::Kind::Runtime ? u"org.freedesktop.Flatpak.runtime-install"_s : u"org.freedesktop.Flatpak.app-install"_s);
    return actions;
}

QStringList systemUpdateActions(const PackageInfo &package)
{
    return {package.kind == PackageInfo::Kind::Runtime ? u"org.freedesktop.Flatpak.runtime-update"_s : u"org.freedesktop.Flatpak.app-update"_s};
}

QString remoteNameForUrl(const QString &remoteUrl, const QByteArray &remotesOutput)
{
    if (remoteUrl.trimmed().isEmpty()) {
        return {};
    }
    const QString wanted = normalizedUrl(remoteUrl);
    const QStringList lines = QString::fromUtf8(remotesOutput).split(u'\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList columns = line.split(u'\t');
        if (columns.size() >= 2 && normalizedUrl(columns.at(1)) == wanted) {
            return columns.at(0).trimmed();
        }
    }
    return {};
}

bool offerSystemInstall(bool hasSystemInstallation, const QList<Authorization> &authorizations, bool steamOS)
{
    if (!hasSystemInstallation) {
        return false;
    }
    bool needsPassword = false;
    for (const Authorization result : authorizations) {
        switch (result) {
        case Authorization::Denied:
            return false;
        case Authorization::NeedsPassword:
        case Authorization::Unknown:
            needsPassword = true;
            break;
        case Authorization::Allowed:
            break;
        }
    }
    // SteamOS's default "deck" account has no password, so an admin
    // prompt could never succeed there.
    return !(needsPassword && steamOS);
}

} // namespace HostEnvironment
