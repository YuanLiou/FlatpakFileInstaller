// SPDX-License-Identifier: Apache-2.0
#include "languagepreference.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace LanguagePreference;

class LanguagePreferenceTest : public QObject
{
    Q_OBJECT

private:
    SavedEnvironment m_dataDirs;

private Q_SLOTS:
    void init()
    {
        m_dataDirs.remember("XDG_DATA_DIRS");
    }

    void cleanup()
    {
        m_dataDirs.restore();
    }

    void availableCodesAlwaysIncludeEnglishFirst()
    {
        QCOMPARE(availableCodes(QSet<QString>{}), QStringList{u"en"_s});
        QCOMPARE(availableCodes(QSet<QString>{u"zh_TW"_s, u"de"_s}), (QStringList{u"en"_s, u"de"_s, u"zh_TW"_s}));
        QCOMPARE(availableCodes(QSet<QString>{u"en"_s, u"ja"_s}), (QStringList{u"en"_s, u"ja"_s}));
        // KI18n lists en_US even without any catalog.
        QCOMPARE(availableCodes(QSet<QString>{u"en_US"_s, u"zh_TW"_s}), (QStringList{u"en"_s, u"zh_TW"_s}));
    }

    void availableCodesFromCatalogsOnDisk()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        const auto touch = [](const QTemporaryDir &dir, const QString &path) {
            const QString file = dir.filePath(path);
            QVERIFY(QDir().mkpath(QFileInfo(file).path()));
            QFile f(file);
            QVERIFY(f.open(QIODevice::WriteOnly));
        };
        touch(first, u"locale/zh_TW/LC_MESSAGES/test-domain.mo"_s);
        touch(first, u"locale/de/LC_MESSAGES/other-domain.mo"_s); // another app's
        touch(second, u"locale/ja/LC_MESSAGES/test-domain.mo"_s);
        QVERIFY(QDir().mkpath(second.filePath(u"locale/fr/LC_MESSAGES"_s))); // no catalog in it

        qputenv("XDG_DATA_DIRS", QFile::encodeName(first.path() + u':' + second.path()));
        QCOMPARE(availableCodes(u"test-domain"_s), (QStringList{u"en"_s, u"ja"_s, u"zh_TW"_s}));
        QCOMPARE(availableCodes(u"no-such-domain"_s), QStringList{u"en"_s});
    }

    void nativeNamesAreInTheirOwnLanguage()
    {
        QCOMPARE(nativeName(u"en"_s), u"English"_s);
        QCOMPARE(nativeName(u"zh_TW"_s), u"繁體中文"_s);
        QCOMPARE(nativeName(u"de"_s), u"Deutsch"_s);
        QCOMPARE(nativeName(u"fr"_s), u"Français"_s);
    }

    void matchLocaleNames_data()
    {
        QTest::addColumn<QString>("locale");
        QTest::addColumn<QString>("expected");

        QTest::newRow("exact") << u"zh_TW"_s << u"zh_TW"_s;
        QTest::newRow("dash") << u"zh-TW"_s << u"zh_TW"_s;
        QTest::newRow("encoding") << u"zh_TW.UTF-8"_s << u"zh_TW"_s;
        QTest::newRow("modifier") << u"zh_TW.UTF-8@euro"_s << u"zh_TW"_s;
        QTest::newRow("script and territory") << u"zh-Hant-TW"_s << u"zh_TW"_s;
        QTest::newRow("traditional script") << u"zh-Hant"_s << u"zh_TW"_s;
        QTest::newRow("simplified is not traditional") << u"zh_CN"_s << QString();
        QTest::newRow("simplified script") << u"zh-Hans"_s << QString();
        QTest::newRow("english territory") << u"en_US.UTF-8"_s << u"en"_s;
        QTest::newRow("english") << u"en"_s << u"en"_s;
        QTest::newRow("case") << u"ZH_tw"_s << u"zh_TW"_s;
        QTest::newRow("C") << u"C"_s << QString();
        QTest::newRow("C.UTF-8") << u"C.UTF-8"_s << QString();
        QTest::newRow("POSIX") << u"POSIX"_s << QString();
        QTest::newRow("empty") << QString() << QString();
        QTest::newRow("no translation") << u"ja_JP"_s << QString();
    }

    void matchLocaleNames()
    {
        QFETCH(QString, locale);
        QFETCH(QString, expected);
        QCOMPARE(match(locale, {u"en"_s, u"zh_TW"_s}), expected);
    }

    void resolveFallsBackToEnglish()
    {
        const QStringList available{u"en"_s, u"zh_TW"_s};
        QCOMPARE(resolve({u"zh_TW"_s, u"en_US"_s}, available), u"zh_TW"_s);
        QCOMPARE(resolve({u"ja_JP"_s, u"zh_TW"_s}, available), u"zh_TW"_s);
        QCOMPARE(resolve({u"ja_JP"_s}, available), u"en"_s);
        QCOMPARE(resolve({}, available), u"en"_s);
        QCOMPARE(resolve({u"C"_s}, available), u"en"_s);
    }

    void systemLanguagesFollowGettextOrder()
    {
        const auto all = [](const QString &) { return true; };
        QCOMPARE(systemLanguages(u"zh_TW:en_US"_s, u""_s, u""_s, u"de_DE.UTF-8"_s, all), (QStringList{u"zh_TW"_s, u"en_US"_s, u"de_DE.UTF-8"_s}));
        QCOMPARE(systemLanguages({}, u"ja_JP"_s, u"de_DE"_s, u"fr_FR"_s, all), QStringList{u"ja_JP"_s});
        QCOMPARE(systemLanguages({}, {}, u"de_DE"_s, u"fr_FR"_s, all), QStringList{u"de_DE"_s});
        QCOMPARE(systemLanguages({}, {}, {}, u"fr_FR"_s, all), QStringList{u"fr_FR"_s});
    }

    void systemLanguagesIgnoredWhereGettextIgnoresThem()
    {
        const auto all = [](const QString &) { return true; };
        const auto none = [](const QString &) { return false; };
        // LANGUAGE does nothing under the C locale...
        QVERIFY(systemLanguages(u"zh_TW"_s, {}, {}, u"C"_s, all).isEmpty());
        QVERIFY(systemLanguages(u"zh_TW"_s, {}, {}, u"C.UTF-8"_s, all).isEmpty());
        QVERIFY(systemLanguages(u"zh_TW"_s, {}, {}, u"POSIX"_s, all).isEmpty());
        QVERIFY(systemLanguages(u"zh_TW"_s, {}, {}, {}, all).isEmpty());
        QVERIFY(systemLanguages(u"zh_TW"_s, u"C"_s, {}, u"zh_TW.UTF-8"_s, all).isEmpty());
        // ...nor under a locale that isn't installed.
        QVERIFY(systemLanguages(u"zh_TW"_s, {}, {}, u"zh_TW.UTF-8"_s, none).isEmpty());
    }

    void replacementLocaleWhenGettextWouldIgnoreLanguage()
    {
        const auto installed = [](const QString &name) { return name == u"en_US.UTF-8"_s || name == u"zh_TW.UTF-8"_s; };
        const auto onlyEnglish = [](const QString &name) { return name == u"en_US.UTF-8"_s; };
        const auto none = [](const QString &) { return false; };

        // A usable locale needs no help.
        QCOMPARE(replacementLocale(u"zh_TW"_s, {}, {}, u"zh_TW.UTF-8"_s, installed), QString());
        QCOMPARE(replacementLocale(u"zh_TW"_s, {}, {}, u"en_US.UTF-8"_s, installed), QString());
        // LANG=C and the like do.
        QCOMPARE(replacementLocale(u"zh_TW"_s, {}, {}, u"C"_s, installed), u"zh_TW.UTF-8"_s);
        QCOMPARE(replacementLocale(u"zh_TW"_s, {}, {}, {}, installed), u"zh_TW.UTF-8"_s);
        QCOMPARE(replacementLocale(u"zh_TW"_s, {}, {}, u"C.UTF-8"_s, installed), u"zh_TW.UTF-8"_s);
        QCOMPARE(replacementLocale(u"zh_TW"_s, {}, {}, u"de_DE.UTF-8"_s, installed), u"zh_TW.UTF-8"_s);
        QCOMPARE(replacementLocale(u"zh_TW"_s, u"C"_s, {}, u"zh_TW.UTF-8"_s, installed), u"zh_TW.UTF-8"_s);
        // Falls back to English's locale, which any system should have.
        QCOMPARE(replacementLocale(u"zh_TW"_s, {}, {}, u"C"_s, onlyEnglish), u"en_US.UTF-8"_s);
        QCOMPARE(replacementLocale(u"zh_TW"_s, {}, {}, u"C"_s, none), QString());
        // English and "follow the system" need nothing.
        QCOMPARE(replacementLocale(u"en"_s, {}, {}, u"C"_s, installed), QString());
        QCOMPARE(replacementLocale({}, {}, {}, u"C"_s, installed), QString());
    }

    void applyChoiceSetsLanguageAndLocale()
    {
        const auto installed = [](const QString &name) { return name == u"zh_TW.UTF-8"_s; };
        SavedEnvironment saved;
        for (const char *name : {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"}) {
            saved.remember(name);
        }

        qputenv("LANG", "C");
        qunsetenv("LC_ALL");
        qunsetenv("LC_MESSAGES");
        qunsetenv("LANGUAGE");
        applyChoice(u"zh_TW"_s, installed);
        QCOMPARE(qgetenv("LANGUAGE"), QByteArray("zh_TW:en_US"));
        QCOMPARE(qgetenv("LC_MESSAGES"), QByteArray("zh_TW.UTF-8"));
        QVERIFY(!qEnvironmentVariableIsSet("LC_ALL"));

        // LC_ALL beats LC_MESSAGES, so that is what gets replaced.
        qputenv("LC_ALL", "C");
        qunsetenv("LC_MESSAGES");
        applyChoice(u"zh_TW"_s, installed);
        QCOMPARE(qgetenv("LC_ALL"), QByteArray("zh_TW.UTF-8"));
        QVERIFY(!qEnvironmentVariableIsSet("LC_MESSAGES"));

        // English is spelled the way KI18n knows to stop at.
        applyChoice(u"en"_s, installed);
        QCOMPARE(qgetenv("LANGUAGE"), QByteArray("en_US"));

        // Nothing chosen: the environment stays as it is.
        qputenv("LANGUAGE", "de");
        applyChoice({}, installed);
        QCOMPARE(qgetenv("LANGUAGE"), QByteArray("de"));

        saved.restore();
    }

    void localeExistsKnowsTheC()
    {
        QVERIFY(localeExists(u"C"_s));
        QVERIFY(!localeExists(u"xx_YY.not-a-locale"_s));
    }

    void storedChoiceRoundTrip()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        const QStringList available{u"en"_s, u"zh_TW"_s};

        QCOMPARE(storedChoice(settings, available), QString());
        storeChoice(settings, u"zh_TW"_s);
        QCOMPARE(storedChoice(settings, available), u"zh_TW"_s);

        // A second reader sees it: it was written out right away.
        QSettings other(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        QCOMPARE(storedChoice(other, available), u"zh_TW"_s);

        storeChoice(settings, u"en"_s);
        QCOMPARE(storedChoice(settings, available), u"en"_s);

        // "Follow the system" is the absence of a value.
        storeChoice(settings, QString());
        QVERIFY(!settings.contains(u"language"_s));
        QCOMPARE(storedChoice(settings, available), QString());
    }

    void storedChoiceWithoutTranslationFollowsTheSystem()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath(u"settings.conf"_s), QSettings::IniFormat);
        storeChoice(settings, u"ja"_s);
        QCOMPARE(storedChoice(settings, {u"en"_s, u"zh_TW"_s}), QString());
        QCOMPARE(storedChoice(settings, {u"en"_s, u"ja"_s}), u"ja"_s);
    }

    void openSettingsUsesFixedNames()
    {
        QTemporaryDir dir;
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, dir.path());
        // Before any QCoreApplication name is set, as in main().
        QVERIFY(QCoreApplication::organizationName().isEmpty());
        const auto settings = openSettings();
        storeChoice(*settings, u"zh_TW"_s);
        QVERIFY(QFileInfo::exists(dir.filePath(u"yuanliou/flatpak-file-installer.conf"_s)));
    }

    void effectiveLanguageOfAChoice()
    {
        const QStringList available{u"en"_s, u"zh_TW"_s};
        // An explicit choice wins over the system.
        QCOMPARE(effectiveLanguage(u"en"_s, {u"zh_TW"_s}, available), u"en"_s);
        QCOMPARE(effectiveLanguage(u"zh_TW"_s, {u"en_US"_s}, available), u"zh_TW"_s);
        // Following the system: its language, or English without a translation.
        QCOMPARE(effectiveLanguage({}, {u"zh_TW"_s}, available), u"zh_TW"_s);
        QCOMPARE(effectiveLanguage({}, {u"ja_JP"_s, u"zh_TW"_s}, available), u"zh_TW"_s);
        QCOMPARE(effectiveLanguage({}, {u"ja_JP"_s}, available), u"en"_s);
        QCOMPARE(effectiveLanguage({}, {}, available), u"en"_s);
        // A choice that has lost its translation is the same as no choice.
        QCOMPARE(effectiveLanguage(u"ja"_s, {u"zh_TW"_s}, available), u"zh_TW"_s);
    }

    void restartArgumentsReplaceTheFile()
    {
        QCOMPARE(restartArguments({u"app"_s}, {}, {}), QStringList{u"app"_s});
        QCOMPARE(restartArguments({u"app"_s}, {}, u"/tmp/a.flatpak"_s), (QStringList{u"app"_s, u"--"_s, u"/tmp/a.flatpak"_s}));
        QCOMPARE(restartArguments({u"app"_s, u"/tmp/a.flatpak"_s}, {u"/tmp/a.flatpak"_s}, u"/tmp/b.flatpak"_s), (QStringList{u"app"_s, u"--"_s, u"/tmp/b.flatpak"_s}));
        // The file is gone from the window: don't open it again.
        QCOMPARE(restartArguments({u"app"_s, u"/tmp/a.flatpak"_s}, {u"/tmp/a.flatpak"_s}, {}), QStringList{u"app"_s});
        // Options stay; a "--" that came with the file doesn't pile up.
        QCOMPARE(restartArguments({u"app"_s, u"--platform"_s, u"xcb"_s, u"--"_s, u"-odd.flatpak"_s}, {u"-odd.flatpak"_s}, u"-odd.flatpak"_s),
                 (QStringList{u"app"_s, u"--platform"_s, u"xcb"_s, u"--"_s, u"-odd.flatpak"_s}));
    }

    void savedEnvironmentRestoresValuesAndAbsence()
    {
        qputenv("FFI_TEST_SET", "original");
        qunsetenv("FFI_TEST_UNSET");

        SavedEnvironment saved;
        saved.remember("FFI_TEST_SET");
        saved.remember("FFI_TEST_UNSET");

        qputenv("FFI_TEST_SET", "changed");
        qputenv("FFI_TEST_UNSET", "changed");
        // Remembering again must not capture the changed values.
        saved.remember("FFI_TEST_SET");
        saved.remember("FFI_TEST_UNSET");

        saved.restore();
        QCOMPARE(qgetenv("FFI_TEST_SET"), QByteArray("original"));
        QVERIFY(!qEnvironmentVariableIsSet("FFI_TEST_UNSET"));
        QCOMPARE(saved.names().size(), 2);
    }

    void savedEnvironmentKeepsAnEmptyValue()
    {
        qputenv("FFI_TEST_EMPTY", "");
        SavedEnvironment saved;
        saved.remember("FFI_TEST_EMPTY");
        qputenv("FFI_TEST_EMPTY", "x");
        saved.restore();
        QVERIFY(qEnvironmentVariableIsSet("FFI_TEST_EMPTY"));
        QVERIFY(qEnvironmentVariableIsEmpty("FFI_TEST_EMPTY"));
    }
};

QTEST_GUILESS_MAIN(LanguagePreferenceTest)
#include "languagepreferencetest.moc"
