// SPDX-License-Identifier: Apache-2.0
#include "flatpakcli.h"

using namespace Qt::StringLiterals;

namespace FlatpakCli
{

namespace
{
QString installationOption(bool forEveryone)
{
    return forEveryone ? u"--system"_s : u"--user"_s;
}

// "id//branch", or just "id" to let flatpak pick the branch.
QString partialRef(const PackageInfo &package)
{
    return package.branch.isEmpty() ? package.id : package.id + u"//"_s + package.branch;
}
}

QString flathubUrl()
{
    return u"https://dl.flathub.org/repo/"_s;
}

QString flathubRepoFile()
{
    return u"https://dl.flathub.org/repo/flathub.flatpakrepo"_s;
}

QString flathubRemoteName()
{
    return u"flathub"_s;
}

QStringList installArguments(const PackageInfo &package, const QString &path, bool forEveryone, bool reinstall)
{
    QStringList arguments{u"install"_s, installationOption(forEveryone), u"--noninteractive"_s, u"--assumeyes"_s};
    if (reinstall) {
        arguments.append(u"--reinstall"_s);
    }
    arguments.append(package.format == PackageInfo::Format::Bundle ? u"--bundle"_s : u"--from"_s);
    arguments.append(path);
    return arguments;
}

QStringList updateArguments(const QString &id, const QString &branch, bool forEveryone)
{
    const QString ref = branch.isEmpty() ? id : id + u"//"_s + branch;
    return {u"update"_s, installationOption(forEveryone), u"--noninteractive"_s, u"--assumeyes"_s, ref};
}

QStringList listArguments(PackageInfo::Kind kind, bool forEveryone)
{
    return {
        u"list"_s,
        installationOption(forEveryone),
        kind == PackageInfo::Kind::Runtime ? u"--runtime"_s : u"--app"_s,
        u"--columns=application,branch,arch,version,active"_s,
    };
}

QStringList remoteCommitArguments(const QString &remote, const PackageInfo &package, bool forEveryone)
{
    return {u"remote-info"_s, installationOption(forEveryone), u"--show-commit"_s, remote, partialRef(package)};
}

QStringList runArguments(const PackageInfo &package, bool forEveryone)
{
    return {u"run"_s, installationOption(forEveryone), partialRef(package)};
}

QStringList addRemoteArguments(const QString &name, const QString &repoFile, bool forEveryone)
{
    return {u"remote-add"_s, installationOption(forEveryone), u"--if-not-exists"_s, name, repoFile};
}

QStringList remotesArguments(bool forEveryone)
{
    return {u"remotes"_s, installationOption(forEveryone), u"--columns=name,url"_s};
}

QStringList remoteInfoArguments(const QString &remote, const PackageInfo &package, bool forEveryone)
{
    return {u"remote-info"_s, installationOption(forEveryone), u"--show-metadata"_s, remote, partialRef(package)};
}

QStringList runtimeInfoArguments(const QString &runtime, bool forEveryone)
{
    return {u"info"_s, installationOption(forEveryone), runtime};
}

} // namespace FlatpakCli
