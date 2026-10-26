// SPDX-License-Identifier: Apache-2.0
#include "packagefile.h"
#include "permissions.h"

#include <QTest>

using namespace Qt::StringLiterals;
using Permissions::Kind;
using Permissions::Permission;

class PermissionsTest : public QObject
{
    Q_OBJECT

private:
    static QList<Kind> kinds(const QList<Permission> &permissions)
    {
        QList<Kind> result;
        for (const Permission &permission : permissions) {
            result.append(permission.kind);
        }
        return result;
    }

private Q_SLOTS:
    void bundleFixture()
    {
        const auto result = PackageFile::load(QStringLiteral(TEST_DATA_DIR "/notes.flatpak"), {});
        QVERIFY(result.info);
        // notes.flatpak: --share=network --socket=wayland --socket=pulseaudio --filesystem=home --device=dri
        QCOMPARE(kinds(Permissions::fromMetadata(result.info->metadata)), (QList<Kind>{Kind::HomeFolder, Kind::Network, Kind::Sound}));
    }

    void plumbingIsLeftOut()
    {
        const QByteArray metadata =
            "[Context]\n"
            "shared=ipc;\n"
            "sockets=wayland;fallback-x11;inherit-wayland-socket;\n"
            "devices=dri;shm;\n"
            "filesystems=xdg-config/kdeglobals:ro;xdg-run/pipewire-0;xdg-data/icons:ro;\n"
            "[Session Bus Policy]\n"
            "org.freedesktop.Notifications=talk\n"
            "org.kde.StatusNotifierWatcher=talk\n";
        // xdg-run/pipewire-0 is writable app data, the only thing left.
        QCOMPARE(kinds(Permissions::fromMetadata(metadata)), QList<Kind>{Kind::OtherAppsData});
    }

    void noContext()
    {
        QVERIFY(Permissions::fromMetadata("[Application]\nname=org.example.App\n").isEmpty());
        QVERIFY(Permissions::fromMetadata(QByteArray()).isEmpty());
        QVERIFY(Permissions::fromMetadata("not a key file").isEmpty());
    }

    void negationsAreIgnored()
    {
        QVERIFY(Permissions::fromMetadata("[Context]\nshared=!network;\nsockets=!x11;\nfilesystems=!home;\n").isEmpty());
    }

    void orderedByImportance()
    {
        const QByteArray metadata =
            "[Context]\n"
            "shared=network;\n"
            "sockets=x11;pulseaudio;cups;\n"
            "devices=all;\n"
            "filesystems=host;\n"
            "[Session Bus Policy]\n"
            "org.freedesktop.Flatpak=talk\n"
            "org.freedesktop.secrets=talk\n";
        QCOMPARE(kinds(Permissions::fromMetadata(metadata)),
                 (QList<Kind>{Kind::RunAnyProgram,
                              Kind::AllFiles,
                              Kind::AllDevices,
                              Kind::Passwords,
                              Kind::LegacyDisplay,
                              Kind::Network,
                              Kind::Sound,
                              Kind::Printing}));
    }

    void folders()
    {
        const QByteArray metadata = "[Context]\nfilesystems=xdg-download;xdg-pictures/Screenshots:ro;~/Games:create;/mnt/data:ro;xdg-music:rw;\n";
        const QList<Permission> permissions = Permissions::fromMetadata(metadata);
        QCOMPARE(permissions.size(), 2);
        QCOMPARE(permissions.at(0), (Permission{.kind = Kind::Folders, .readOnly = false, .folders = {u"xdg-download"_s, u"~/Games"_s, u"xdg-music"_s}}));
        QCOMPARE(permissions.at(1), (Permission{.kind = Kind::Folders, .readOnly = true, .folders = {u"xdg-pictures/Screenshots"_s, u"/mnt/data"_s}}));
    }

    void wholeHomeCoversItsFolders()
    {
        const QByteArray metadata = "[Context]\nfilesystems=home;xdg-download;~/Games:ro;/mnt/data;\n";
        const QList<Permission> permissions = Permissions::fromMetadata(metadata);
        QCOMPARE(permissions.size(), 2);
        QCOMPARE(permissions.at(0).kind, Kind::HomeFolder);
        QCOMPARE(permissions.at(1).folders, QStringList{u"/mnt/data"_s});
    }

    void readOnlyHomeStillListsWritableFolders()
    {
        const QByteArray metadata = "[Context]\nfilesystems=home:ro;xdg-download;xdg-music:ro;\n";
        const QList<Permission> permissions = Permissions::fromMetadata(metadata);
        QCOMPARE(permissions.size(), 2);
        QCOMPARE(permissions.at(0), (Permission{.kind = Kind::HomeFolder, .readOnly = true, .folders = {}}));
        QCOMPARE(permissions.at(1), (Permission{.kind = Kind::Folders, .readOnly = false, .folders = {u"xdg-download"_s}}));
    }

    void allFilesCoversHome()
    {
        const QList<Permission> permissions = Permissions::fromMetadata("[Context]\nfilesystems=home;host;xdg-download;\n");
        QCOMPARE(kinds(permissions), QList<Kind>{Kind::AllFiles});
        QVERIFY(!permissions.at(0).readOnly);
    }

    void writableWinsOverReadOnly()
    {
        const QList<Permission> permissions = Permissions::fromMetadata("[Context]\nfilesystems=host-os:ro;host-etc;\n");
        QCOMPARE(permissions, (QList<Permission>{{.kind = Kind::SystemFiles, .readOnly = false, .folders = {}}}));
    }

    void busPolicyNoneIsIgnored()
    {
        QVERIFY(Permissions::fromMetadata("[Session Bus Policy]\norg.freedesktop.Flatpak=none\n").isEmpty());
    }

    void needsTrust()
    {
        QVERIFY(Permissions::needsTrust(Kind::RunAnyProgram));
        QVERIFY(Permissions::needsTrust(Kind::HomeFolder));
        QVERIFY(Permissions::needsTrust(Kind::LegacyDisplay));
        QVERIFY(!Permissions::needsTrust(Kind::Folders));
        QVERIFY(!Permissions::needsTrust(Kind::Network));
        QVERIFY(!Permissions::needsTrust(Kind::Sound));
    }

    void source()
    {
        PackageInfo bundle;
        bundle.format = PackageInfo::Format::Bundle;
        bundle.remoteUrl = u"https://dl.flathub.org/repo/"_s;
        QCOMPARE(Permissions::sourceOf(bundle), Permissions::Source::File);

        PackageInfo ref;
        ref.format = PackageInfo::Format::Ref;
        ref.remoteUrl = u"https://dl.flathub.org/repo/"_s;
        QCOMPARE(Permissions::sourceOf(ref), Permissions::Source::Flathub);
        ref.remoteUrl = u"https://flathub.example.com/repo/"_s;
        QCOMPARE(Permissions::sourceOf(ref), Permissions::Source::OtherSite);
    }
};

QTEST_GUILESS_MAIN(PermissionsTest)
#include "permissionstest.moc"
