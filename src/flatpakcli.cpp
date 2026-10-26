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
}

QStringList installArguments(const PackageInfo &package, const QString &path, bool forEveryone)
{
    return {
        u"install"_s,
        installationOption(forEveryone),
        u"--noninteractive"_s,
        u"--assumeyes"_s,
        package.format == PackageInfo::Format::Bundle ? u"--bundle"_s : u"--from"_s,
        path,
    };
}

QStringList runArguments(const PackageInfo &package, bool forEveryone)
{
    QString ref = package.id;
    if (!package.branch.isEmpty()) {
        ref += u"//"_s + package.branch;
    }
    return {u"run"_s, installationOption(forEveryone), ref};
}

} // namespace FlatpakCli
