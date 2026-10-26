// SPDX-License-Identifier: Apache-2.0
#include "hostcommand.h"

#include <QFile>
#include <QProcess>

using namespace Qt::StringLiterals;

namespace HostCommand
{

bool runningInFlatpak()
{
    return QFile::exists(u"/.flatpak-info"_s);
}

Command wrap(const QString &program, const QStringList &arguments, bool inFlatpak)
{
    if (!inFlatpak) {
        return {.program = program, .arguments = arguments};
    }
    return {.program = u"flatpak-spawn"_s, .arguments = QStringList{u"--host"_s, program} + arguments};
}

QProcessEnvironment cleanEnvironment(const QProcessEnvironment &env, const QString &appDir)
{
    if (appDir.isEmpty()) {
        return env;
    }

    QProcessEnvironment cleaned;
    const QStringList keys = env.keys();
    for (const QString &key : keys) {
        const QString value = env.value(key);
        if (!value.contains(appDir)) {
            cleaned.insert(key, value);
            continue;
        }
        // PATH-like lists: keep the host's entries, drop ours.
        QStringList kept;
        const QStringList entries = value.split(u':');
        for (const QString &entry : entries) {
            if (!entry.isEmpty() && !entry.startsWith(appDir)) {
                kept.append(entry);
            }
        }
        if (!kept.isEmpty()) {
            cleaned.insert(key, kept.join(u':'));
        }
    }
    for (const auto *key : {"APPDIR", "APPIMAGE", "ARGV0", "OWD"}) {
        cleaned.remove(QString::fromLatin1(key));
    }
    return cleaned;
}

QProcess *create(const QString &program, const QStringList &arguments, QObject *parent)
{
    const Command command = wrap(program, arguments, runningInFlatpak());
    auto *process = new QProcess(parent);
    process->setProgram(command.program);
    process->setArguments(command.arguments);
    process->setProcessEnvironment(cleanEnvironment(QProcessEnvironment::systemEnvironment(), qEnvironmentVariable("APPDIR")));
    return process;
}

} // namespace HostCommand
