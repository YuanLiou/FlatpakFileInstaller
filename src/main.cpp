// SPDX-License-Identifier: Apache-2.0
#include "installer.h"
#include "languagepreference.h"
#include "preferences.h"

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    // Remember what the environment looked like, so that restarting to change
    // the language can start again from it.
    Preferences::Startup startup;
    for (const char *name : {"LANGUAGE", "LC_ALL", "LC_MESSAGES"}) {
        startup.environment.remember(name);
    }
    startup.environment.remember("XDG_DATA_DIRS");
    startup.systemLanguages = LanguagePreference::systemLanguages();

    // In an AppImage our translations live under $APPDIR/usr/share.
    if (const QByteArray appDir = qgetenv("APPDIR"); !appDir.isEmpty()) {
        QByteArray dataDirs = qgetenv("XDG_DATA_DIRS");
        if (dataDirs.isEmpty()) {
            dataDirs = "/usr/local/share:/usr/share";
        }
        qputenv("XDG_DATA_DIRS", QByteArray(appDir + "/usr/share:" + dataDirs));
    }

    // The language chosen in the settings. KI18n, Kirigami and Qt all read
    // the environment when they start, so it has to be set before QApplication.
    startup.available = LanguagePreference::availableCodes(QStringLiteral(TRANSLATION_DOMAIN));
    const QString chosen = Preferences::languageOverride(startup.available);
    LanguagePreference::applyChoice(chosen);
    startup.language = LanguagePreference::effectiveLanguage(chosen, startup.systemLanguages, startup.available);

    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain(QByteArrayLiteral(TRANSLATION_DOMAIN));

    KAboutData about(u"flatpak-file-installer"_s,
                     i18n("Flatpak File Installer"),
                     QStringLiteral(APP_VERSION),
                     i18n("Install apps you downloaded from the internet"),
                     KAboutLicense::Apache_V2,
                     u"© 2026 Jui Yuan Liu (Ray Yuan)"_s);
    about.setDesktopFileName(QStringLiteral(APP_ID));
    about.setHomepage(u"https://github.com/YuanLiou/FlatpakFileInstaller"_s);
    about.setBugAddress("https://github.com/YuanLiou/FlatpakFileInstaller/issues");
    about.addAuthor(u"Jui Yuan Liu (Ray Yuan)"_s);
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(APP_ID), QIcon(u":/icons/app.svg"_s)));

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    parser.addPositionalArgument(u"file"_s, i18n("The .flatpak or .flatpakref file to install"), u"[file]"_s);
    parser.process(app);
    about.processCommandLine(&parser);

    Installer installer(nullptr);
    Installer::setInstance(&installer);

    startup.arguments = app.arguments();
    startup.files = parser.positionalArguments();
    Preferences preferences(startup, &installer, nullptr);
    Preferences::setInstance(&preferences);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("io.github.yuanliou.FlatpakFileInstaller", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (const QStringList files = parser.positionalArguments(); !files.isEmpty()) {
        installer.open(QUrl::fromUserInput(files.first(), QDir::currentPath(), QUrl::AssumeLocalFile));
    }
    return app.exec();
}
