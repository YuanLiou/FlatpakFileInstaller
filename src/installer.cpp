// SPDX-License-Identifier: Apache-2.0
#include "installer.h"

#include "flatpakcli.h"
#include "hostcommand.h"
#include "packagefile.h"

#include <KFormat>
#include <KLocalizedString>

#include <QJSEngine>
#include <QQmlEngine>

using namespace Qt::StringLiterals;

namespace
{
Installer *s_instance = nullptr;
constexpr int kCommandNotFound = 127;
}

Installer::Installer(QObject *parent)
    : QObject(parent)
{
}

Installer *Installer::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_ASSERT(s_instance);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void Installer::setInstance(Installer *instance)
{
    s_instance = instance;
}

Installer::State Installer::state() const
{
    return m_state;
}

bool Installer::isBusy() const
{
    return m_state == State::Checking || m_state == State::Installing;
}

QString Installer::appName() const
{
    return m_package.displayName();
}

QString Installer::appSummary() const
{
    return m_package.summary;
}

QString Installer::appVersion() const
{
    return m_package.version;
}

QString Installer::appDeveloper() const
{
    return m_package.developer;
}

QVariant Installer::appIcon() const
{
    if (m_package.icon.isNull()) {
        return u"qrc:/icons/package.svg"_s;
    }
    return m_package.icon;
}

QString Installer::appSize() const
{
    if (m_package.installedSize == 0) {
        return {};
    }
    // "KB"/"MB" rather than "KiB"/"MiB": more familiar to most people.
    return KFormat().formatByteSize(static_cast<double>(m_package.installedSize), 1, KFormat::JEDECBinaryDialect);
}

bool Installer::canLaunch() const
{
    return m_package.kind == PackageInfo::Kind::App;
}

bool Installer::isSystemAvailable() const
{
    return m_systemAvailable;
}

bool Installer::isInstalledForEveryone() const
{
    return m_installedForEveryone;
}

QString Installer::errorMessage() const
{
    return m_errorMessage;
}

QString Installer::errorDetails() const
{
    return m_errorDetails;
}

void Installer::open(const QUrl &file)
{
    if (isBusy()) {
        return;
    }
    ++m_generation;
    m_package = {};
    m_path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    m_systemAvailable = false;
    m_installedForEveryone = false;

    const PackageFile::Result result = PackageFile::load(m_path, KLocalizedString::languages());
    Q_EMIT packageChanged();
    if (!result.info) {
        fail(State::Invalid,
             result.error == PackageFile::Error::Unreadable
                 ? i18n("This file can't be opened. It may have been moved or deleted.")
                 : i18n("This file isn't an app that can be installed here. Look for a download that ends in “.flatpak” or “.flatpakref”."));
        return;
    }

    m_package = *result.info;
    Q_EMIT packageChanged();
    setState(State::Checking);
    checkSystemInstall();
}

void Installer::checkSystemInstall()
{
    runOnHost(u"flatpak"_s, {u"--installations"_s}, [this](const Output &installations) {
        if (!installations.ranNormally || installations.exitCode == kCommandNotFound) {
            fail(State::FlatpakMissing,
                 i18n("This computer can't install Flatpak apps yet, because the Flatpak software isn't installed."),
                 QString::fromUtf8(installations.standardError).trimmed());
            return;
        }
        m_hasSystemInstallation = installations.exitCode == 0 && !installations.standardOutput.trimmed().isEmpty();
        if (!m_hasSystemInstallation) {
            m_systemAvailable = false;
            setState(State::Ready);
            return;
        }

        if (m_package.format == PackageInfo::Format::Bundle) {
            checkAuthorizations(HostEnvironment::systemInstallActions(m_package, false), {});
            return;
        }
        runOnHost(u"flatpak"_s, {u"remotes"_s, u"--system"_s, u"--columns=url"_s}, [this](const Output &remotes) {
            const bool configured = remotes.exitCode == 0 && HostEnvironment::isRemoteConfigured(m_package.remoteUrl, remotes.standardOutput);
            checkAuthorizations(HostEnvironment::systemInstallActions(m_package, configured), {});
        });
    });
}

void Installer::checkAuthorizations(QStringList actions, QList<HostEnvironment::Authorization> results)
{
    if (actions.isEmpty()) {
        const bool steamOS = HostEnvironment::isSteamOS(HostEnvironment::readHostOsRelease(HostCommand::runningInFlatpak()));
        m_systemAvailable = HostEnvironment::offerSystemInstall(m_hasSystemInstallation, results, steamOS);
        setState(State::Ready);
        return;
    }

    const QString action = actions.takeFirst();
    // pkcheck asks about a process; `exec` makes the shell's PID pkcheck's own.
    const QStringList arguments{u"-c"_s, u"exec pkcheck --action-id \"$1\" --process $$"_s, u"sh"_s, action};
    runOnHost(u"sh"_s, arguments, [this, actions, results](const Output &output) mutable {
        results.append(HostEnvironment::authorizationFromPkcheck(output.exitCode, output.ranNormally));
        checkAuthorizations(actions, results);
    });
}

void Installer::install(bool forEveryone)
{
    if (m_state != State::Ready || (forEveryone && !m_systemAvailable)) {
        return;
    }
    m_installedForEveryone = forEveryone;
    setState(State::Installing);
    runOnHost(u"flatpak"_s, FlatpakCli::installArguments(m_package, m_path, forEveryone), [this](const Output &output) {
        if (output.ranNormally && output.exitCode == 0) {
            setState(State::Installed);
            return;
        }
        const QString details = QString::fromUtf8(output.standardError + output.standardOutput).trimmed();
        fail(State::Failed, i18n("“%1” couldn't be installed.", m_package.displayName()), details);
    });
}

void Installer::launch()
{
    if (m_state != State::Installed || !canLaunch()) {
        return;
    }
    QProcess *process = HostCommand::create(u"flatpak"_s, FlatpakCli::runArguments(m_package, m_installedForEveryone), nullptr);
    process->startDetached();
    delete process;
}

void Installer::tryAgain()
{
    if (m_state == State::Failed) {
        m_errorMessage.clear();
        m_errorDetails.clear();
        setState(State::Ready);
    }
}

void Installer::runOnHost(const QString &program, const QStringList &arguments, Callback done)
{
    QProcess *process = HostCommand::create(program, arguments, this);
    const quint64 generation = m_generation;
    auto finish = [this, process, generation, done](bool ranNormally) {
        Output output{
            .exitCode = process->exitCode(),
            .ranNormally = ranNormally,
            .standardOutput = process->readAllStandardOutput(),
            .standardError = process->readAllStandardError(),
        };
        process->deleteLater();
        if (generation == m_generation) {
            done(output);
        }
    };
    connect(process, &QProcess::finished, this, [finish](int, QProcess::ExitStatus status) {
        finish(status == QProcess::NormalExit);
    });
    connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError error) {
        // Crashes are reported through finished(); only startup needs handling here.
        if (error == QProcess::FailedToStart) {
            finish(false);
        }
    });
    process->start();
}

void Installer::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        Q_EMIT stateChanged();
    }
}

void Installer::fail(State state, const QString &message, const QString &details)
{
    m_errorMessage = message;
    m_errorDetails = details;
    m_state = state;
    Q_EMIT stateChanged();
}
