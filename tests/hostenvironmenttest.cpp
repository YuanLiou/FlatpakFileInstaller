// SPDX-License-Identifier: Apache-2.0
#include "hostenvironment.h"

#include <QTest>

using namespace Qt::StringLiterals;
using HostEnvironment::Authorization;

class HostEnvironmentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesOsRelease()
    {
        const auto os = HostEnvironment::parseOsRelease(
            "NAME=\"SteamOS\"\n"
            "# comment\n"
            "PRETTY_NAME=\"SteamOS\"\n"
            "ID=steamos\n"
            "ID_LIKE=arch\n");
        QCOMPARE(os.id, u"steamos"_s);
        QCOMPARE(os.idLike, QStringList{u"arch"_s});
        QCOMPARE(os.prettyName, u"SteamOS"_s);
        QVERIFY(HostEnvironment::isSteamOS(os));
    }

    void fedoraIsNotSteamOS()
    {
        const auto os = HostEnvironment::parseOsRelease("ID=fedora\nVARIANT_ID=kinoite\nPRETTY_NAME='Fedora Linux 44 (Kinoite)'\n");
        QCOMPARE(os.id, u"fedora"_s);
        QCOMPARE(os.prettyName, u"Fedora Linux 44 (Kinoite)"_s);
        QVERIFY(!HostEnvironment::isSteamOS(os));
    }

    void pkcheckExitCodes_data()
    {
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<bool>("ranNormally");
        QTest::addColumn<Authorization>("expected");
        QTest::newRow("authorized") << 0 << true << Authorization::Allowed;
        QTest::newRow("not authorized") << 1 << true << Authorization::Denied;
        QTest::newRow("challenge") << 2 << true << Authorization::NeedsPassword;
        QTest::newRow("dismissed") << 3 << true << Authorization::Unknown;
        QTest::newRow("error") << 127 << true << Authorization::Unknown;
        QTest::newRow("did not start") << 0 << false << Authorization::Unknown;
    }

    void pkcheckExitCodes()
    {
        QFETCH(int, exitCode);
        QFETCH(bool, ranNormally);
        QFETCH(Authorization, expected);
        QCOMPARE(HostEnvironment::authorizationFromPkcheck(exitCode, ranNormally), expected);
    }

    void actionsForBundle()
    {
        PackageInfo bundle;
        bundle.format = PackageInfo::Format::Bundle;
        HostEnvironment::InstallationState system;
        QCOMPARE(HostEnvironment::systemInstallActions(bundle, system), QStringList{u"org.freedesktop.Flatpak.install-bundle"_s});

        system.runtimeInstalled = false;
        system.flathubConfigured = true;
        QCOMPARE(HostEnvironment::systemInstallActions(bundle, system),
                 (QStringList{u"org.freedesktop.Flatpak.install-bundle"_s, u"org.freedesktop.Flatpak.runtime-install"_s}));

        system.flathubConfigured = false;
        QCOMPARE(HostEnvironment::systemInstallActions(bundle, system),
                 (QStringList{u"org.freedesktop.Flatpak.install-bundle"_s,
                              u"org.freedesktop.Flatpak.runtime-install"_s,
                              u"org.freedesktop.Flatpak.configure-remote"_s}));
    }

    void actionsForRef()
    {
        PackageInfo ref;
        ref.format = PackageInfo::Format::Ref;
        HostEnvironment::InstallationState system;
        system.remoteConfigured = true;
        QCOMPARE(HostEnvironment::systemInstallActions(ref, system), QStringList{u"org.freedesktop.Flatpak.app-install"_s});
        system.remoteConfigured = false;
        QCOMPARE(HostEnvironment::systemInstallActions(ref, system),
                 (QStringList{u"org.freedesktop.Flatpak.configure-remote"_s, u"org.freedesktop.Flatpak.app-install"_s}));
        system.remoteConfigured = true;
        ref.kind = PackageInfo::Kind::Runtime;
        QCOMPARE(HostEnvironment::systemInstallActions(ref, system), QStringList{u"org.freedesktop.Flatpak.runtime-install"_s});
    }

    void needsFlathub_data()
    {
        QTest::addColumn<PackageInfo::Format>("format");
        QTest::addColumn<QString>("runtimeRepo");
        QTest::addColumn<bool>("runtimeInstalled");
        QTest::addColumn<bool>("flathubConfigured");
        QTest::addColumn<bool>("expected");

        const auto bundle = PackageInfo::Format::Bundle;
        const QString flathubRepo = u"https://dl.flathub.org/repo/flathub.flatpakrepo"_s;
        QTest::newRow("runtime missing, no Flathub") << bundle << QString() << false << false << true;
        QTest::newRow("runtime installed") << bundle << QString() << true << false << false;
        QTest::newRow("Flathub already there") << bundle << QString() << false << true << false;
        QTest::newRow("bundle brings its runtime repo") << bundle << flathubRepo << false << false << false;
        QTest::newRow("flatpakref") << PackageInfo::Format::Ref << QString() << false << false << false;
    }

    void needsFlathub()
    {
        QFETCH(PackageInfo::Format, format);
        QFETCH(QString, runtimeRepo);
        QFETCH(bool, runtimeInstalled);
        QFETCH(bool, flathubConfigured);
        QFETCH(bool, expected);

        PackageInfo package;
        package.format = format;
        package.runtime = u"org.kde.Platform/x86_64/6.10"_s;
        package.runtimeRepoUrl = runtimeRepo;
        HostEnvironment::InstallationState state;
        state.runtimeInstalled = runtimeInstalled;
        state.flathubConfigured = flathubConfigured;
        QCOMPARE(HostEnvironment::needsFlathub(package, state), expected);
    }

    void needsFlathubOnlyForApps()
    {
        PackageInfo runtime;
        runtime.format = PackageInfo::Format::Bundle;
        runtime.kind = PackageInfo::Kind::Runtime;
        HostEnvironment::InstallationState state;
        state.runtimeInstalled = false;
        QVERIFY(!HostEnvironment::needsFlathub(runtime, state));
    }

    void remoteNameForUrl()
    {
        const QByteArray remotes = "flathub\thttps://dl.flathub.org/repo/\nfedora\toci+https://registry.fedoraproject.org\n";
        QCOMPARE(HostEnvironment::remoteNameForUrl(u"https://dl.flathub.org/repo/"_s, remotes), u"flathub"_s);
        QCOMPARE(HostEnvironment::remoteNameForUrl(u"https://dl.flathub.org/repo"_s, remotes), u"flathub"_s);
        QCOMPARE(HostEnvironment::remoteNameForUrl(u"HTTPS://DL.FLATHUB.ORG/repo/"_s, remotes), u"flathub"_s);
        QCOMPARE(HostEnvironment::remoteNameForUrl(u"oci+https://registry.fedoraproject.org/"_s, remotes), u"fedora"_s);
        QVERIFY(HostEnvironment::remoteNameForUrl(u"https://flathub.org/beta-repo/"_s, remotes).isEmpty());
        QVERIFY(HostEnvironment::remoteNameForUrl(QString(), remotes).isEmpty());
        QVERIFY(HostEnvironment::remoteNameForUrl(u"https://dl.flathub.org/repo/"_s, QByteArray()).isEmpty());
        // A line without a name column is not a match.
        QVERIFY(HostEnvironment::remoteNameForUrl(u"https://dl.flathub.org/repo/"_s, "https://dl.flathub.org/repo/\n").isEmpty());
    }

    void offerSystemInstall_data()
    {
        QTest::addColumn<bool>("hasSystemInstallation");
        QTest::addColumn<QList<Authorization>>("authorizations");
        QTest::addColumn<bool>("steamOS");
        QTest::addColumn<bool>("expected");

        const QList<Authorization> allowed{Authorization::Allowed};
        const QList<Authorization> password{Authorization::NeedsPassword};
        const QList<Authorization> denied{Authorization::Denied};
        const QList<Authorization> unknown{Authorization::Unknown};
        const QList<Authorization> mixed{Authorization::Allowed, Authorization::NeedsPassword};

        QTest::newRow("no system installation") << false << allowed << false << false;
        QTest::newRow("allowed") << true << allowed << false << true;
        QTest::newRow("password on Fedora") << true << password << false << true;
        QTest::newRow("password on SteamOS") << true << password << true << false;
        QTest::newRow("allowed on SteamOS") << true << allowed << true << true;
        QTest::newRow("denied") << true << denied << false << false;
        QTest::newRow("unknown on Fedora") << true << unknown << false << true;
        QTest::newRow("unknown on SteamOS") << true << unknown << true << false;
        QTest::newRow("remote needs password on SteamOS") << true << mixed << true << false;
    }

    void offerSystemInstall()
    {
        QFETCH(bool, hasSystemInstallation);
        QFETCH(QList<Authorization>, authorizations);
        QFETCH(bool, steamOS);
        QFETCH(bool, expected);
        QCOMPARE(HostEnvironment::offerSystemInstall(hasSystemInstallation, authorizations, steamOS), expected);
    }
};

QTEST_GUILESS_MAIN(HostEnvironmentTest)
#include "hostenvironmenttest.moc"
