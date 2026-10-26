// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "hostenvironment.h"
#include "packageinfo.h"

#include <QObject>
#include <QProcess>
#include <QUrl>
#include <QVariant>
#include <QtQmlIntegration>

#include <functional>

class QQmlEngine;
class QJSEngine;

// The installer window's model: loads one file, works out where it can be
// installed, and runs the installation on the host.
class Installer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY stateChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY packageChanged)
    Q_PROPERTY(QString appSummary READ appSummary NOTIFY packageChanged)
    Q_PROPERTY(QString appVersion READ appVersion NOTIFY packageChanged)
    Q_PROPERTY(QString appDeveloper READ appDeveloper NOTIFY packageChanged)
    // The bundle's icon as a QImage, or a generic icon's URL.
    Q_PROPERTY(QVariant appIcon READ appIcon NOTIFY packageChanged)
    Q_PROPERTY(QString appSize READ appSize NOTIFY packageChanged)
    Q_PROPERTY(bool canLaunch READ canLaunch NOTIFY packageChanged)
    Q_PROPERTY(bool systemAvailable READ isSystemAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool installedForEveryone READ isInstalledForEveryone NOTIFY stateChanged)
    // The install also downloads a runtime the app needs, so it takes longer.
    Q_PROPERTY(bool downloadsExtras READ downloadsExtras NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QString errorDetails READ errorDetails NOTIFY stateChanged)

public:
    enum class State {
        Empty, // no file opened yet
        Invalid, // file can't be read or isn't a Flatpak
        FlatpakMissing, // host has no `flatpak`
        Checking, // asking the host where we may install
        Ready, // waiting for the user to drag the app
        Installing,
        Installed,
        Failed,
    };
    Q_ENUM(State)

    // No default argument: QML would otherwise construct its own instance
    // instead of calling create().
    explicit Installer(QObject *parent);

    // QML gets the instance created in main().
    static Installer *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static void setInstance(Installer *instance);

    State state() const;
    bool isBusy() const;
    QString appName() const;
    QString appSummary() const;
    QString appVersion() const;
    QString appDeveloper() const;
    QVariant appIcon() const;
    QString appSize() const;
    bool canLaunch() const;
    bool isSystemAvailable() const;
    bool isInstalledForEveryone() const;
    bool downloadsExtras() const;
    QString errorMessage() const;
    QString errorDetails() const;

    Q_INVOKABLE void open(const QUrl &file);
    Q_INVOKABLE void install(bool forEveryone);
    Q_INVOKABLE void launch();
    Q_INVOKABLE void tryAgain();

Q_SIGNALS:
    void stateChanged();
    void packageChanged();

private:
    struct Output {
        int exitCode = -1;
        bool ranNormally = false;
        QByteArray standardOutput;
        QByteArray standardError;
    };
    using Callback = std::function<void(const Output &)>;

    void runOnHost(const QString &program, const QStringList &arguments, Callback done);
    void setState(State state);
    void fail(State state, const QString &message, const QString &details = {});
    void checkInstallations();
    void checkRuntime();
    void checkRemotes();
    void checkAuthorizations(QStringList actions, QList<HostEnvironment::Authorization> results);
    void runInstall(bool forEveryone);
    void failInstall(const Output &output);

    State m_state = State::Empty;
    PackageInfo m_package;
    QString m_path;
    bool m_hasSystemInstallation = false;
    HostEnvironment::InstallationState m_userState;
    HostEnvironment::InstallationState m_systemState;
    bool m_systemAvailable = false;
    bool m_installedForEveryone = false;
    bool m_downloadsExtras = false;
    QString m_errorMessage;
    QString m_errorDetails;
    quint64 m_generation = 0; // ignores results of checks for a previous file
};
