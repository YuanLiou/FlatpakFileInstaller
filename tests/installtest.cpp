// SPDX-License-Identifier: Apache-2.0
//
// Installs the test bundle with the real `flatpak` command into a throwaway
// user installation. Skipped when flatpak or busybox isn't available.
#include "flatpakcli.h"
#include "hostcommand.h"
#include "hostenvironment.h"
#include "packagefile.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class InstallTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    // Each test gets its own empty user and system installations.
    QProcessEnvironment environment(const QString &installation) const
    {
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(u"FLATPAK_USER_DIR"_s, m_dir.filePath(installation + u"/user"_s));
        env.insert(u"FLATPAK_SYSTEM_DIR"_s, m_dir.filePath(installation + u"/system"_s));
        return env;
    }

    // Runs flatpak the same way the app does, plus our test environment.
    QProcess *flatpak(const QStringList &arguments, const QString &installation)
    {
        QProcess *process = HostCommand::create(u"flatpak"_s, arguments, this);
        process->setProcessEnvironment(environment(installation));
        process->start();
        process->waitForFinished(120000);
        return process;
    }

private Q_SLOTS:
    void initTestCase()
    {
        if (QStandardPaths::findExecutable(u"flatpak"_s).isEmpty() || QStandardPaths::findExecutable(u"busybox"_s).isEmpty()) {
            QSKIP("needs flatpak and busybox");
        }
        QVERIFY(m_dir.isValid());
        QProcess fixtures;
        fixtures.start(u"sh"_s, {QStringLiteral(TEST_DATA_DIR "/make-fixtures.sh"), m_dir.path()});
        QVERIFY(fixtures.waitForFinished(120000));
        QCOMPARE(fixtures.exitCode(), 0);
    }

    void installsBundleForUser()
    {
        PackageInfo runtime;
        runtime.format = PackageInfo::Format::Bundle;
        const QString installation = u"bundle"_s;
        QProcess *runtimeInstall = flatpak(FlatpakCli::installArguments(runtime, m_dir.filePath(u"runtime.flatpak"_s), false), installation);
        QVERIFY2(runtimeInstall->exitCode() == 0, runtimeInstall->readAllStandardError().constData());

        // Freshly built so it matches this machine's architecture.
        const QString bundle = m_dir.filePath(u"notes-offline.flatpak"_s);
        const auto result = PackageFile::load(bundle, {});
        QVERIFY(result.info);
        QProcess *install = flatpak(FlatpakCli::installArguments(*result.info, bundle, false), installation);
        QVERIFY2(install->exitCode() == 0, install->readAllStandardError().constData());

        QProcess *list = flatpak({u"list"_s, u"--user"_s, u"--app"_s, u"--columns=application"_s}, installation);
        QCOMPARE(list->readAllStandardOutput().trimmed(), QByteArray("org.example.Notes"));
    }

    // The steps Installer takes when the bundle's runtime is missing, with a
    // local repository standing in for Flathub.
    void downloadsMissingRuntimeFromRemote()
    {
        const QString installation = u"missing-runtime"_s;
        const QString bundle = m_dir.filePath(u"notes-offline.flatpak"_s);
        const auto result = PackageFile::load(bundle, {});
        QVERIFY(result.info);
        const PackageInfo &package = *result.info;

        HostEnvironment::InstallationState state;
        state.runtimeInstalled = flatpak(FlatpakCli::runtimeInfoArguments(package.runtime, false), installation)->exitCode() == 0;
        state.flathubConfigured = HostEnvironment::isRemoteConfigured(
            FlatpakCli::flathubUrl(), flatpak(FlatpakCli::remotesArguments(false), installation)->readAllStandardOutput());
        QVERIFY(!state.runtimeInstalled);
        QVERIFY(HostEnvironment::needsFlathub(package, state));

        QProcess *addRemote = flatpak(FlatpakCli::addRemoteArguments(u"testhub"_s, m_dir.filePath(u"hub.flatpakrepo"_s), false), installation);
        QVERIFY2(addRemote->exitCode() == 0, addRemote->readAllStandardError().constData());
        // Adding it again must be harmless.
        QCOMPARE(flatpak(FlatpakCli::addRemoteArguments(u"testhub"_s, m_dir.filePath(u"hub.flatpakrepo"_s), false), installation)->exitCode(), 0);

        QProcess *install = flatpak(FlatpakCli::installArguments(package, bundle, false), installation);
        QVERIFY2(install->exitCode() == 0, install->readAllStandardError().constData());
        QCOMPARE(flatpak(FlatpakCli::runtimeInfoArguments(package.runtime, false), installation)->exitCode(), 0);
    }

    // A user install can use a runtime that is only installed system-wide,
    // which is why Installer treats that runtime as present for "Just for me".
    void userInstallUsesSystemRuntime()
    {
        const QString installation = u"system-runtime"_s;
        PackageInfo runtime;
        runtime.format = PackageInfo::Format::Bundle;
        QProcess *runtimeInstall = flatpak(FlatpakCli::installArguments(runtime, m_dir.filePath(u"runtime.flatpak"_s), true), installation);
        QVERIFY2(runtimeInstall->exitCode() == 0, runtimeInstall->readAllStandardError().constData());

        const QString bundle = m_dir.filePath(u"notes-offline.flatpak"_s);
        const auto result = PackageFile::load(bundle, {});
        QVERIFY(result.info);
        QProcess *install = flatpak(FlatpakCli::installArguments(*result.info, bundle, false), installation);
        QVERIFY2(install->exitCode() == 0, install->readAllStandardError().constData());
        QVERIFY(flatpak(FlatpakCli::runtimeInfoArguments(result.info->runtime, false), installation)->exitCode() != 0);
    }
};

QTEST_GUILESS_MAIN(InstallTest)
#include "installtest.moc"
