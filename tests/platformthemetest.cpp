// SPDX-License-Identifier: Apache-2.0
#include "platformtheme.h"

#include <QTest>

using namespace Qt::StringLiterals;



class PlatformThemeTest : public QObject
{
    Q_OBJECT

private:
    static bool all(const QByteArray &) { return true; }
    static bool none(const QByteArray &) { return false; }
    static bool onlyPortal(const QByteArray &name) { return name == "xdgdesktopportal"; }

private Q_SLOTS:
    void kdeInTheDesktopList()
    {
        QVERIFY(PlatformTheme::isKde(u"KDE"_s));
        QVERIFY(PlatformTheme::isKde(u"kde"_s));
        QVERIFY(PlatformTheme::isKde(u"ubuntu:KDE"_s));
        QVERIFY(!PlatformTheme::isKde(u"GNOME"_s));
        QVERIFY(!PlatformTheme::isKde(u"X-Cinnamon"_s));
        QVERIFY(!PlatformTheme::isKde(QString()));
    }

    void appImageOnKdeUsesPlasmaThemeWithPortalDialogs()
    {
        const auto choice = PlatformTheme::choose({}, u"KDE"_s, false, true, all, true);
        QCOMPARE(choice.theme, QByteArray("kde"));
        QVERIFY(choice.plasmaUsesPortal);
    }

    void appImageOnKdeWithoutPlasmaThemeUsesThePortalTheme()
    {
        const auto choice = PlatformTheme::choose({}, u"KDE"_s, false, true, onlyPortal, true);
        QCOMPARE(choice.theme, QByteArray("xdgdesktopportal"));
        QVERIFY(!choice.plasmaUsesPortal);
    }

    void appImageElsewhereKeepsThePortalTheme()
    {
        for (const QString &desktop : {u"GNOME"_s, u"XFCE"_s, QString()}) {
            const auto choice = PlatformTheme::choose({}, desktop, false, true, all, true);
            QCOMPARE(choice.theme, QByteArray("xdgdesktopportal"));
            QVERIFY(!choice.plasmaUsesPortal);
        }
    }

    void appImageReplacesTheHooksGtk2()
    {
        QCOMPARE(PlatformTheme::choose("gtk2", u"GNOME"_s, false, true, all, true).theme, QByteArray("xdgdesktopportal"));
        QCOMPARE(PlatformTheme::choose("gtk2", u"KDE"_s, false, true, all, true).theme, QByteArray("kde"));
    }

    void usersChoiceIsKept()
    {
        QVERIFY(PlatformTheme::choose("qt6ct", u"KDE"_s, false, true, all, true).theme.isEmpty());
        QVERIFY(PlatformTheme::choose("kde", u"GNOME"_s, false, true, all, true).theme.isEmpty());
        QVERIFY(PlatformTheme::choose("gtk2", u"KDE"_s, true, false, all, true).theme.isEmpty()); // not ours to replace
    }

    void nothingToDoWithoutPlugins()
    {
        QVERIFY(PlatformTheme::choose({}, u"GNOME"_s, false, true, none, true).theme.isEmpty());
        QVERIFY(PlatformTheme::choose({}, u"KDE"_s, false, true, none, true).theme.isEmpty());
    }

    void plainInstallIsUntouched()
    {
        QVERIFY(PlatformTheme::choose({}, u"KDE"_s, false, false, all, true).theme.isEmpty());
        QVERIFY(PlatformTheme::choose({}, u"GNOME"_s, false, false, all, true).theme.isEmpty());
    }

    void withoutAPortalPlasmaThemeIsNotUsed()
    {
        // Its file dialog would not open; the portal theme falls back to Qt's.
        QCOMPARE(PlatformTheme::choose({}, u"KDE"_s, false, true, all, false).theme, QByteArray("xdgdesktopportal"));
        QVERIFY(PlatformTheme::choose({}, u"KDE"_s, true, false, none, false).theme.isEmpty());
    }

    void flatpakOnKdeAsksForPlasmaTheme()
    {
        const auto choice = PlatformTheme::choose({}, u"KDE"_s, true, false, none, true);
        QCOMPARE(choice.theme, QByteArray("kde"));
        QVERIFY(choice.plasmaUsesPortal);
    }

    void flatpakElsewhereLeavesQtToPickThePortalTheme()
    {
        QVERIFY(PlatformTheme::choose({}, u"GNOME"_s, true, false, none, true).theme.isEmpty());
    }
};

QTEST_GUILESS_MAIN(PlatformThemeTest)
#include "platformthemetest.moc"
