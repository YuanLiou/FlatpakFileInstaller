// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

class QProcess;
class QObject;

namespace LanguagePreference
{
class SavedEnvironment;
}

// Runs programs on the host system.
//
// Inside the Flatpak sandbox this goes through `flatpak-spawn --host`
// (needs --talk-name=org.freedesktop.Flatpak). From an AppImage the program
// runs directly, with the AppImage's own library paths removed so host
// tools such as `flatpak` don't load our bundled libraries.
namespace HostCommand
{

struct Command {
    QString program;
    QStringList arguments;
};

bool runningInFlatpak();

Command wrap(const QString &program, const QStringList &arguments, bool inFlatpak);

// Drops entries pointing into the mounted AppImage (`appDir`) from the
// environment. Returns `env` unchanged when `appDir` is empty.
QProcessEnvironment cleanEnvironment(const QProcessEnvironment &env, const QString &appDir);

// Creates (but does not start) a process that runs `program` on the host.
// With `original`, the variables it remembers get the values they had when
// we started, so an app we launch doesn't inherit what we set for ourselves
// (the language, the platform theme).
QProcess *create(const QString &program, const QStringList &arguments, QObject *parent, const LanguagePreference::SavedEnvironment *original = nullptr);

} // namespace HostCommand
