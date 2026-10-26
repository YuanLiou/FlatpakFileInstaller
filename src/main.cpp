// SPDX-License-Identifier: Apache-2.0
#include "installer.h"

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
    // In an AppImage our translations live under $APPDIR/usr/share.
    if (const QByteArray appDir = qgetenv("APPDIR"); !appDir.isEmpty()) {
        QByteArray dataDirs = qgetenv("XDG_DATA_DIRS");
        if (dataDirs.isEmpty()) {
            dataDirs = "/usr/local/share:/usr/share";
        }
        qputenv("XDG_DATA_DIRS", QByteArray(appDir + "/usr/share:" + dataDirs));
    }

    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain(QByteArrayLiteral(TRANSLATION_DOMAIN));

    KAboutData about(u"flatpak-app-installer"_s,
                     i18n("Flatpak App Installer"),
                     QStringLiteral(APP_VERSION),
                     i18n("Install apps you downloaded from the internet"),
                     KAboutLicense::Apache_V2);
    about.setDesktopFileName(QStringLiteral(APP_ID));
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

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("io.github.yuanliou.FlatpakAppInstaller", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (const QStringList files = parser.positionalArguments(); !files.isEmpty()) {
        installer.open(QUrl::fromUserInput(files.first(), QDir::currentPath(), QUrl::AssumeLocalFile));
    }
    return app.exec();
}
