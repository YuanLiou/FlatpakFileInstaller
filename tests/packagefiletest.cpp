// SPDX-License-Identifier: Apache-2.0
#include "packagefile.h"

#include <QColor>
#include <QFile>
#include <QTest>

using namespace Qt::StringLiterals;

class PackageFileTest : public QObject
{
    Q_OBJECT

private:
    static QString fixture(const char *name)
    {
        return QStringLiteral(TEST_DATA_DIR "/") + QString::fromLatin1(name);
    }

private Q_SLOTS:
    void bundleWithAppStream()
    {
        const auto result = PackageFile::load(fixture("notes.flatpak"), {u"en_US"_s, u"en"_s});
        QCOMPARE(result.error, PackageFile::Error::None);
        QVERIFY(result.info);
        const PackageInfo &info = *result.info;
        QCOMPARE(info.format, PackageInfo::Format::Bundle);
        QCOMPARE(info.kind, PackageInfo::Kind::App);
        QCOMPARE(info.id, u"org.example.Notes"_s);
        QCOMPARE(info.branch, u"stable"_s);
        QVERIFY(!info.arch.isEmpty());
        QCOMPARE(info.name, u"Notes"_s);
        QCOMPARE(info.summary, u"Write things down"_s);
        QCOMPARE(info.developer, u"Example Inc."_s);
        QCOMPARE(info.version, u"1.2.3"_s);
        QVERIFY(info.runtime.startsWith(u"org.test.Platform/"_s));
        QCOMPARE(info.runtimeRepoUrl, u"https://dl.flathub.org/repo/flathub.flatpakrepo"_s);
        QCOMPARE(info.icon.size(), QSize(64, 64));
        QVERIFY(info.installedSize > 0);
        // What `flatpak list --columns=active` shows once it is installed.
        QCOMPARE(info.commit, u"f6b8ec462e0d574c08c130161cb2f035130c93d6394a3bfea044f3010c390297"_s);
    }

    void bundlePicksPreferredLanguage()
    {
        const auto result = PackageFile::load(fixture("notes.flatpak"), {u"zh_TW"_s, u"zh"_s});
        QVERIFY(result.info);
        QCOMPARE(result.info->name, u"筆記本"_s);
        QCOMPARE(result.info->summary, u"把事情記下來"_s);
    }

    void bundleLanguageAcceptsBcp47Form()
    {
        const auto result = PackageFile::load(fixture("notes.flatpak"), {u"zh-TW"_s});
        QVERIFY(result.info);
        QCOMPARE(result.info->name, u"筆記本"_s);
    }

    void localizedValue_data()
    {
        QTest::addColumn<QStringList>("languages");
        QTest::addColumn<QString>("expected");
        QTest::newRow("bcp47 key") << QStringList{u"zh_TW"_s} << u"繁體"_s;
        QTest::newRow("simplified") << QStringList{u"zh_CN"_s} << u"简体"_s;
        QTest::newRow("bare language skips regional keys") << QStringList{u"zh"_s} << u"Default"_s;
        QTest::newRow("falls through to next language") << QStringList{u"ja_JP"_s, u"zh_TW"_s} << u"繁體"_s;
        QTest::newRow("no match") << QStringList{u"fr_FR"_s} << u"Default"_s;
    }

    void localizedValue()
    {
        QFETCH(QStringList, languages);
        QFETCH(QString, expected);
        const QHash<QString, QString> values{
            {QString(), u"Default"_s},
            {u"zh_Hant_TW"_s, u"繁體"_s},
            {u"zh_Hans_CN"_s, u"简体"_s},
        };
        QCOMPARE(PackageFile::localizedValue(values, languages), expected);
    }

    void bundleWithoutAppStreamFallsBackToId()
    {
        const auto result = PackageFile::load(fixture("hello.flatpak"), {u"en"_s});
        QVERIFY(result.info);
        QCOMPARE(result.info->id, u"org.spike.Hello"_s);
        QVERIFY(result.info->name.isEmpty());
        QCOMPARE(result.info->displayName(), u"Hello"_s);
        QVERIFY(result.info->icon.isNull());
    }

    void bundleWithoutAppStreamUsesExports()
    {
        const auto result = PackageFile::load(fixture("paint.flatpak"), {u"en_US"_s, u"en"_s});
        QVERIFY(result.info);
        QCOMPARE(result.info->name, u"Paint"_s);
        QCOMPARE(result.info->summary, u"Draw pictures"_s);
        // The SVG is drawn at the size the interface needs.
        QCOMPARE(result.info->icon.size(), QSize(256, 256));
        QCOMPARE(result.info->icon.pixelColor(128, 128), QColor(0x33, 0x99, 0xff));
    }

    void bundleExportsPickPreferredLanguage()
    {
        const auto result = PackageFile::load(fixture("paint.flatpak"), {u"zh_TW"_s, u"zh"_s});
        QVERIFY(result.info);
        QCOMPARE(result.info->name, u"小畫家"_s);
        QCOMPARE(result.info->summary, u"畫圖"_s);
    }

    void bundleExportsPickBiggestPng()
    {
        // The 128px icon is a symlink into files/, in another part of the
        // bundle than the directory listings.
        const auto result = PackageFile::load(fixture("sketch.flatpak"), {u"en"_s});
        QVERIFY(result.info);
        QCOMPARE(result.info->name, u"Sketch"_s);
        QCOMPARE(result.info->icon.size(), QSize(128, 128));
    }

    void ref()
    {
        const auto result = PackageFile::load(fixture("example.flatpakref"), {});
        QCOMPARE(result.error, PackageFile::Error::None);
        QVERIFY(result.info);
        const PackageInfo &info = *result.info;
        QCOMPARE(info.format, PackageInfo::Format::Ref);
        QCOMPARE(info.kind, PackageInfo::Kind::App);
        QCOMPARE(info.id, u"org.example.Notes"_s);
        QCOMPARE(info.branch, u"stable"_s);
        QCOMPARE(info.name, u"Example Notes"_s);
        QCOMPARE(info.summary, u"Write things down"_s);
        QCOMPARE(info.remoteUrl, u"https://dl.flathub.org/repo/"_s);
        QCOMPARE(info.runtimeRepoUrl, u"https://dl.flathub.org/repo/flathub.flatpakrepo"_s);
        QCOMPARE(info.iconUrl, u"https://dl.flathub.org/repo/appstream/x86_64/icons/128x128/org.example.Notes.png"_s);
        QVERIFY(info.icon.isNull());
    }

    void refRuntime()
    {
        const auto result = PackageFile::parseRef("[Flatpak Ref]\nName=org.kde.Platform\nBranch=6.10\nUrl=https://dl.flathub.org/repo/\nIsRuntime=true\n");
        QVERIFY(result.info);
        QCOMPARE(result.info->kind, PackageInfo::Kind::Runtime);
    }

    void rejectsInvalidRefs_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::newRow("no group") << QByteArray("Name=org.example.App\nUrl=https://example.org/repo\n");
        QTest::newRow("no url") << QByteArray("[Flatpak Ref]\nName=org.example.App\n");
        QTest::newRow("bad id") << QByteArray("[Flatpak Ref]\nName=--system\nUrl=https://example.org/repo\n");
        QTest::newRow("two part id") << QByteArray("[Flatpak Ref]\nName=example.App\nUrl=https://example.org/repo\n");
        QTest::newRow("bad branch") << QByteArray("[Flatpak Ref]\nName=org.example.App\nBranch=a b\nUrl=https://example.org/repo\n");
    }

    void rejectsInvalidRefs()
    {
        QFETCH(QByteArray, contents);
        const auto result = PackageFile::parseRef(contents);
        QVERIFY(!result.info);
        QCOMPARE(result.error, PackageFile::Error::NotFlatpak);
    }

    void rejectsGarbage_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("text") << QByteArray("hello world");
        QTest::newRow("binary") << QByteArray(4096, '\xff');
        QByteArray truncated;
        QFile notes(fixture("notes.flatpak"));
        if (notes.open(QIODevice::ReadOnly)) {
            truncated = notes.read(notes.size() / 2);
        }
        QTest::newRow("truncated bundle") << truncated;
    }

    void rejectsGarbage()
    {
        QFETCH(QByteArray, contents);
        const auto result = PackageFile::parseBundle(contents, {});
        QVERIFY(!result.info);
        QCOMPARE(result.error, PackageFile::Error::NotFlatpak);
    }

    void installableFileNames_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<bool>("expected");
        QTest::newRow("bundle") << u"app.flatpak"_s << true;
        QTest::newRow("ref") << u"app.flatpakref"_s << true;
        QTest::newRow("upper case") << u"App.FLATPAKREF"_s << true;
        QTest::newRow("repo file") << u"flathub.flatpakrepo"_s << false;
        QTest::newRow("other") << u"app.AppImage"_s << false;
        QTest::newRow("no extension") << u"flatpak"_s << false;
    }

    void installableFileNames()
    {
        QFETCH(QString, fileName);
        QFETCH(bool, expected);
        QCOMPARE(PackageFile::isInstallableFileName(fileName), expected);
    }

    void missingFile()
    {
        const auto result = PackageFile::load(fixture("does-not-exist.flatpak"), {});
        QCOMPARE(result.error, PackageFile::Error::Unreadable);
    }
};

QTEST_GUILESS_MAIN(PackageFileTest)
#include "packagefiletest.moc"
