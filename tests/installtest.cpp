// SPDX-License-Identifier: Apache-2.0
//
// Installs the test bundle with the real `flatpak` command into a throwaway
// user installation. Skipped when flatpak or busybox isn't available.
#include "flatpakcli.h"
#include "hostcommand.h"
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

    QProcessEnvironment environment() const
    {
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(u"FLATPAK_USER_DIR"_s, m_dir.filePath(u"installation"_s));
        return env;
    }

    // Runs flatpak the same way the app does, plus our test environment.
    QProcess *flatpak(const QStringList &arguments)
    {
        QProcess *process = HostCommand::create(u"flatpak"_s, arguments, this);
        process->setProcessEnvironment(environment());
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
        QProcess *runtimeInstall = flatpak(FlatpakCli::installArguments(runtime, m_dir.filePath(u"runtime.flatpak"_s), false));
        QVERIFY2(runtimeInstall->exitCode() == 0, runtimeInstall->readAllStandardError().constData());

        // Freshly built so it matches this machine's architecture.
        const QString bundle = m_dir.filePath(u"notes-offline.flatpak"_s);
        const auto result = PackageFile::load(bundle, {});
        QVERIFY(result.info);
        QProcess *install = flatpak(FlatpakCli::installArguments(*result.info, bundle, false));
        QVERIFY2(install->exitCode() == 0, install->readAllStandardError().constData());

        QProcess *list = flatpak({u"list"_s, u"--user"_s, u"--app"_s, u"--columns=application"_s});
        QCOMPARE(list->readAllStandardOutput().trimmed(), QByteArray("org.example.Notes"));
    }
};

QTEST_GUILESS_MAIN(InstallTest)
#include "installtest.moc"
