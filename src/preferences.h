// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "languagepreference.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQmlIntegration>

class Installer;
class QQmlEngine;
class QJSEngine;

// The settings page's model: which language the interface uses.
//
// A new language only takes effect when the app starts, so choosing one
// saves it right away and offers to restart.
class Preferences : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // The languages to choose from: a list of {code, name} maps, starting
    // with "follow the system" (empty code).
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    // The saved choice; empty when following the system.
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    // The question asked after choosing a language, in that language.
    Q_PROPERTY(QString restartTitle READ restartTitle NOTIFY restartPrompted)
    Q_PROPERTY(QString restartExplanation READ restartExplanation NOTIFY restartPrompted)
    Q_PROPERTY(QString restartNowText READ restartNowText NOTIFY restartPrompted)
    Q_PROPERTY(QString restartLaterText READ restartLaterText NOTIFY restartPrompted)

public:
    // What main() worked out before QApplication was created.
    struct Startup {
        QStringList available; // language codes with a translation
        QStringList systemLanguages; // what the system asks for, without our override
        QString language; // the language this process runs in
        QStringList arguments; // the command line, program name first
        QStringList files; // the files on that command line
        LanguagePreference::SavedEnvironment environment;
    };

    // No default argument: see Installer.
    explicit Preferences(const Startup &startup, Installer *installer, QObject *parent);

    static Preferences *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static void setInstance(Preferences *instance);

    // The saved choice, to be put in the environment at startup; empty for
    // "follow the system", which leaves the environment alone.
    static QString languageOverride(const QStringList &available);

    QVariantList languages() const;
    QString language() const;
    QString restartTitle() const;
    QString restartExplanation() const;
    QString restartNowText() const;
    QString restartLaterText() const;

    // Saves `code` ("" for the system's language). Emits restartPrompted()
    // when the language it leads to isn't the one in use.
    Q_INVOKABLE void chooseLanguage(const QString &code);
    // Starts the app again in place, opening the same file. Only returns
    // (false) if it couldn't, or while the installer is busy.
    Q_INVOKABLE bool restart();

Q_SIGNALS:
    void languageChanged();
    void restartPrompted();

private:
    Installer *m_installer;
    Startup m_startup;
    std::unique_ptr<QSettings> m_settings;
    QString m_language;
    QString m_target;
};
