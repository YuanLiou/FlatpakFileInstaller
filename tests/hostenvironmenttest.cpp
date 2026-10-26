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
        QCOMPARE(HostEnvironment::systemInstallActions(bundle, true), QStringList{u"org.freedesktop.Flatpak.install-bundle"_s});
    }

    void actionsForRef()
    {
        PackageInfo ref;
        ref.format = PackageInfo::Format::Ref;
        QCOMPARE(HostEnvironment::systemInstallActions(ref, true), QStringList{u"org.freedesktop.Flatpak.app-install"_s});
        QCOMPARE(HostEnvironment::systemInstallActions(ref, false),
                 (QStringList{u"org.freedesktop.Flatpak.configure-remote"_s, u"org.freedesktop.Flatpak.app-install"_s}));
        ref.kind = PackageInfo::Kind::Runtime;
        QCOMPARE(HostEnvironment::systemInstallActions(ref, true), QStringList{u"org.freedesktop.Flatpak.runtime-install"_s});
    }

    void remoteConfigured()
    {
        const QByteArray remotes = "https://dl.flathub.org/repo/\nhttps://registry.fedoraproject.org/\n";
        QVERIFY(HostEnvironment::isRemoteConfigured(u"https://dl.flathub.org/repo/"_s, remotes));
        QVERIFY(HostEnvironment::isRemoteConfigured(u"https://dl.flathub.org/repo"_s, remotes));
        QVERIFY(HostEnvironment::isRemoteConfigured(u"HTTPS://DL.FLATHUB.ORG/repo/"_s, remotes));
        QVERIFY(!HostEnvironment::isRemoteConfigured(u"https://flathub.org/beta-repo/"_s, remotes));
        QVERIFY(!HostEnvironment::isRemoteConfigured(QString(), remotes));
        QVERIFY(!HostEnvironment::isRemoteConfigured(u"https://dl.flathub.org/repo/"_s, QByteArray()));
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
