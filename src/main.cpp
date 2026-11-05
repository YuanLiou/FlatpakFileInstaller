// SPDX-License-Identifier: Apache-2.0
#include "hostcommand.h"
#include "installer.h"
#include "languagepreference.h"
#include "platformtheme.h"
#include "preferences.h"

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QUrl>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    // Remember what the environment looked like, so that restarting to change
    // the language can start again from it.
    Preferences::Startup startup;
    for (const char *name : {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "QT_QPA_PLATFORMTHEME", "PLASMA_INTEGRATION_USE_PORTAL"}) {
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

    // Which platform theme to use. Without one Qt shows its own QML file dialog
    // and no desktop settings. In an AppImage we carry the themes (the portal's,
    // and Plasma's on KDE); linuxdeploy's hook sets "gtk2" on GNOME and XFCE,
    // which isn't bundled and so does nothing: that one is replaced. In a Flatpak
    // on KDE we ask for Plasma's theme (Qt would use the portal's). A value the
    // user set is theirs.
    {
        const QString appDirPath = QString::fromUtf8(qgetenv("APPDIR"));
        const auto hasPlugin = [&appDirPath](const QByteArray &name) {
            const QString file = name == "kde" ? u"KDEPlasmaPlatformTheme6.so"_s : u"libqxdgdesktopportal.so"_s;
            return QFile::exists(appDirPath + u"/usr/plugins/platformthemes/"_s + file);
        };
        // In a Flatpak the portal is always there. Otherwise look for the files
        // a desktop portal installs: asking D-Bus would mean connecting before
        // QApplication exists.
        const bool inFlatpak = HostCommand::runningInFlatpak();
        const bool kdePortalInstalled = !QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"xdg-desktop-portal/portals/kde.portal"_s).isEmpty()
            && !QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"dbus-1/services/org.freedesktop.portal.Desktop.service"_s).isEmpty();
        const PlatformTheme::Choice choice = PlatformTheme::choose(qgetenv("QT_QPA_PLATFORMTHEME"),
                                                                   qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                                                                   inFlatpak,
                                                                   !appDirPath.isEmpty(),
                                                                   hasPlugin,
                                                                   inFlatpak || kdePortalInstalled);
        if (!choice.theme.isEmpty()) {
            qputenv("QT_QPA_PLATFORMTHEME", choice.theme);
            if (choice.plasmaUsesPortal && !qEnvironmentVariableIsSet("PLASMA_INTEGRATION_USE_PORTAL")) {
                qputenv("PLASMA_INTEGRATION_USE_PORTAL", "1");
            }
        }
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
    installer.setLaunchEnvironment(startup.environment);

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
