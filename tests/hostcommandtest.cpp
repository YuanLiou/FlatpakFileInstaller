// SPDX-License-Identifier: Apache-2.0
#include "flatpakcli.h"
#include "hostcommand.h"

#include <QTest>

using namespace Qt::StringLiterals;

class HostCommandTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void wrapOutsideFlatpak()
    {
        const auto command = HostCommand::wrap(u"flatpak"_s, {u"--installations"_s}, false);
        QCOMPARE(command.program, u"flatpak"_s);
        QCOMPARE(command.arguments, QStringList{u"--installations"_s});
    }

    void wrapInsideFlatpak()
    {
        const auto command = HostCommand::wrap(u"flatpak"_s, {u"--installations"_s}, true);
        QCOMPARE(command.program, u"flatpak-spawn"_s);
        QCOMPARE(command.arguments, (QStringList{u"--host"_s, u"flatpak"_s, u"--installations"_s}));
    }

    void cleanEnvironmentRemovesAppImagePaths()
    {
        QProcessEnvironment env;
        env.insert(u"APPDIR"_s, u"/tmp/.mount_abc"_s);
        env.insert(u"APPIMAGE"_s, u"/home/me/Installer.AppImage"_s);
        env.insert(u"LD_LIBRARY_PATH"_s, u"/tmp/.mount_abc/usr/lib"_s);
        env.insert(u"PATH"_s, u"/tmp/.mount_abc/usr/bin:/usr/bin:/bin"_s);
        env.insert(u"XDG_DATA_DIRS"_s, u"/tmp/.mount_abc/usr/share:/usr/share"_s);
        env.insert(u"HOME"_s, u"/home/me"_s);

        const auto cleaned = HostCommand::cleanEnvironment(env, u"/tmp/.mount_abc"_s);
        QVERIFY(!cleaned.contains(u"APPDIR"_s));
        QVERIFY(!cleaned.contains(u"APPIMAGE"_s));
        QVERIFY(!cleaned.contains(u"LD_LIBRARY_PATH"_s));
        QCOMPARE(cleaned.value(u"PATH"_s), u"/usr/bin:/bin"_s);
        QCOMPARE(cleaned.value(u"XDG_DATA_DIRS"_s), u"/usr/share"_s);
        QCOMPARE(cleaned.value(u"HOME"_s), u"/home/me"_s);
    }

    void cleanEnvironmentWithoutAppImage()
    {
        QProcessEnvironment env;
        env.insert(u"PATH"_s, u"/usr/bin"_s);
        QCOMPARE(HostCommand::cleanEnvironment(env, QString()), env);
    }

    void installArguments()
    {
        PackageInfo bundle;
        bundle.format = PackageInfo::Format::Bundle;
        QCOMPARE(FlatpakCli::installArguments(bundle, u"/run/user/1000/doc/ab12/app.flatpak"_s, false),
                 (QStringList{u"install"_s, u"--user"_s, u"--noninteractive"_s, u"--assumeyes"_s, u"--bundle"_s, u"/run/user/1000/doc/ab12/app.flatpak"_s}));

        PackageInfo ref;
        ref.format = PackageInfo::Format::Ref;
        QCOMPARE(FlatpakCli::installArguments(ref, u"/tmp/app.flatpakref"_s, true),
                 (QStringList{u"install"_s, u"--system"_s, u"--noninteractive"_s, u"--assumeyes"_s, u"--from"_s, u"/tmp/app.flatpakref"_s}));
    }

    void remoteArguments()
    {
        QCOMPARE(FlatpakCli::addRemoteArguments(FlatpakCli::flathubRemoteName(), FlatpakCli::flathubRepoFile(), false),
                 (QStringList{u"remote-add"_s, u"--user"_s, u"--if-not-exists"_s, u"flathub"_s, u"https://dl.flathub.org/repo/flathub.flatpakrepo"_s}));
        QCOMPARE(FlatpakCli::remotesArguments(true), (QStringList{u"remotes"_s, u"--system"_s, u"--columns=name,url"_s}));
        PackageInfo app;
        app.id = u"org.example.Notes"_s;
        app.branch = u"stable"_s;
        QCOMPARE(FlatpakCli::remoteInfoArguments(u"flathub"_s, app, true),
                 (QStringList{u"remote-info"_s, u"--system"_s, u"--show-metadata"_s, u"flathub"_s, u"org.example.Notes//stable"_s}));
        QCOMPARE(FlatpakCli::runtimeInfoArguments(u"org.kde.Platform/x86_64/6.10"_s, false),
                 (QStringList{u"info"_s, u"--user"_s, u"org.kde.Platform/x86_64/6.10"_s}));
    }

    void runArguments()
    {
        PackageInfo app;
        app.id = u"org.example.Notes"_s;
        app.branch = u"stable"_s;
        QCOMPARE(FlatpakCli::runArguments(app, false), (QStringList{u"run"_s, u"--user"_s, u"org.example.Notes//stable"_s}));
        app.branch.clear();
        QCOMPARE(FlatpakCli::runArguments(app, true), (QStringList{u"run"_s, u"--system"_s, u"org.example.Notes"_s}));
    }
};

QTEST_GUILESS_MAIN(HostCommandTest)
#include "hostcommandtest.moc"
