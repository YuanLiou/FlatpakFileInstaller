// SPDX-License-Identifier: Apache-2.0
#include "existinginstall.h"

#include <QTest>

using namespace Qt::StringLiterals;
using ExistingInstall::Comparison;
using ExistingInstall::Installed;

class ExistingInstallTest : public QObject
{
    Q_OBJECT

private:
    static PackageInfo bundle(const QString &version, const QString &commit)
    {
        PackageInfo package;
        package.format = PackageInfo::Format::Bundle;
        package.id = u"org.example.Notes"_s;
        package.arch = u"x86_64"_s;
        package.branch = u"stable"_s;
        package.version = version;
        package.commit = commit;
        return package;
    }

    static Installed installed(const QString &version, const QString &commit)
    {
        return {.forEveryone = false, .branch = u"stable"_s, .version = version, .commit = commit};
    }

private Q_SLOTS:
    void findInList()
    {
        const QByteArray list =
            "org.example.Other\tstable\tx86_64\t1.0\taaaaaaaaaaaa\n"
            "org.example.Notes\tbeta\tx86_64\t2.0\tbbbbbbbbbbbb\n"
            "org.example.Notes\tstable\taarch64\t1.0\tcccccccccccc\n"
            "org.example.Notes\tstable\tx86_64\t1.2.3\tcde2a4892791\n";
        const auto found = ExistingInstall::findInList(bundle(u"1.2.3"_s, {}), list, true);
        QVERIFY(found);
        QCOMPARE(found->forEveryone, true);
        QCOMPARE(found->branch, u"stable"_s);
        QCOMPARE(found->version, u"1.2.3"_s);
        QCOMPARE(found->commit, u"cde2a4892791"_s);
    }

    void findInListWithoutBranch()
    {
        // A .flatpakref may leave the branch out.
        PackageInfo ref;
        ref.format = PackageInfo::Format::Ref;
        ref.id = u"org.example.Notes"_s;
        const auto found = ExistingInstall::findInList(ref, "org.example.Notes\tstable\tx86_64\t\tcde2a4892791\n", false);
        QVERIFY(found);
        QVERIFY(found->version.isEmpty());
    }

    void notInList()
    {
        QVERIFY(!ExistingInstall::findInList(bundle({}, {}), "org.example.Other\tstable\tx86_64\t1.0\taaaaaaaaaaaa\n", false));
        QVERIFY(!ExistingInstall::findInList(bundle({}, {}), QByteArray(), false));
        // Not an ID match just because it starts the same.
        QVERIFY(!ExistingInstall::findInList(bundle({}, {}), "org.example.NotesPlus\tstable\tx86_64\t1.0\taaaaaaaaaaaa\n", false));
        QVERIFY(!ExistingInstall::findInList(bundle({}, {}), "garbage\n", false));
    }

    void compareBundle_data()
    {
        QTest::addColumn<QString>("fileVersion");
        QTest::addColumn<QString>("fileCommit");
        QTest::addColumn<QString>("installedVersion");
        QTest::addColumn<QString>("installedCommit");
        QTest::addColumn<Comparison>("expected");

        const QString commit = u"cde2a489279116b4cfb59651b8f7a81c86c32c05add011556a2ee01eff531b5e"_s;
        const QString other = u"f6b8ec462e0d574c08c130161cb2f035130c93d6394a3bfea044f3010c390297"_s;
        QTest::newRow("same build") << u"1.0"_s << commit << u"9.9"_s << commit.left(12) << Comparison::SameVersion;
        QTest::newRow("newer file") << u"1.10"_s << commit << u"1.9"_s << other.left(12) << Comparison::FileIsNewer;
        QTest::newRow("older file") << u"1.2.3"_s << commit << u"2.0"_s << other.left(12) << Comparison::FileIsOlder;
        QTest::newRow("same number, new build") << u"1.0"_s << commit << u"1.0"_s << other.left(12) << Comparison::SameVersion;
        QTest::newRow("no version in file") << QString() << commit << u"1.0"_s << other.left(12) << Comparison::DifferentVersion;
        QTest::newRow("no installed version") << u"1.0"_s << commit << QString() << other.left(12) << Comparison::DifferentVersion;
        QTest::newRow("short commits never match") << QString() << u"cde2"_s << QString() << u"cde2"_s << Comparison::DifferentVersion;
    }

    void compareBundle()
    {
        QFETCH(QString, fileVersion);
        QFETCH(QString, fileCommit);
        QFETCH(QString, installedVersion);
        QFETCH(QString, installedCommit);
        QFETCH(Comparison, expected);
        QCOMPARE(ExistingInstall::compare(bundle(fileVersion, fileCommit), installed(installedVersion, installedCommit)), expected);
    }

    void compareRef()
    {
        PackageInfo ref;
        ref.format = PackageInfo::Format::Ref;
        const Installed copy = installed(u"1.0"_s, u"cde2a4892791"_s);
        QCOMPARE(ExistingInstall::compare(ref, copy), Comparison::Unknown);
        QCOMPARE(ExistingInstall::compare(ref, copy, u"cde2a489279116b4cfb59651b8f7a81c86c32c05add011556a2ee01eff531b5e"_s), Comparison::UpToDate);
        QCOMPARE(ExistingInstall::compare(ref, copy, u"f6b8ec462e0d574c08c130161cb2f035130c93d6394a3bfea044f3010c390297"_s), Comparison::UpdateAvailable);
    }

    void compareVersions_data()
    {
        QTest::addColumn<QString>("a");
        QTest::addColumn<QString>("b");
        QTest::addColumn<int>("expected");
        QTest::newRow("equal") << u"1.2.3"_s << u"1.2.3"_s << 0;
        QTest::newRow("numeric, not text") << u"1.10"_s << u"1.9"_s << 1;
        QTest::newRow("major") << u"2.0"_s << u"1.99.99"_s << 1;
        QTest::newRow("leading zeros") << u"1.01"_s << u"1.1"_s << 0;
        QTest::newRow("more parts") << u"1.0.1"_s << u"1.0"_s << 1;
        QTest::newRow("number beats letters") << u"1.2"_s << u"1.beta"_s << 1;
        QTest::newRow("letters") << u"1.0b"_s << u"1.0a"_s << 1;
        QTest::newRow("separators ignored") << u"2026-01-02"_s << u"2026.1.2"_s << 0;
        QTest::newRow("huge numbers") << u"20260101120000"_s << u"9"_s << 1;
    }

    void compareVersions()
    {
        QFETCH(QString, a);
        QFETCH(QString, b);
        QFETCH(int, expected);
        QCOMPARE(ExistingInstall::compareVersions(a, b), expected);
        QCOMPARE(ExistingInstall::compareVersions(b, a), -expected);
    }
};

QTEST_GUILESS_MAIN(ExistingInstallTest)
#include "existinginstalltest.moc"
