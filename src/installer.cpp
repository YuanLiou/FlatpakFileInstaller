// SPDX-License-Identifier: Apache-2.0
#include "installer.h"

#include "flathubclient.h"
#include "flatpakcli.h"
#include "hostcommand.h"
#include "icondownloader.h"
#include "packagefile.h"
#include "permissiontext.h"

#include <KFormat>
#include <KLocalizedString>

#include <QJSEngine>
#include <QQmlEngine>

using namespace Qt::StringLiterals;

namespace
{
Installer *s_instance = nullptr;
constexpr int kCommandNotFound = 127;

Installer::Existing toExisting(ExistingInstall::Comparison comparison)
{
    switch (comparison) {
    case ExistingInstall::Comparison::SameVersion:
        return Installer::Existing::SameVersion;
    case ExistingInstall::Comparison::FileIsNewer:
        return Installer::Existing::FileIsNewer;
    case ExistingInstall::Comparison::FileIsOlder:
        return Installer::Existing::FileIsOlder;
    case ExistingInstall::Comparison::DifferentVersion:
        return Installer::Existing::DifferentVersion;
    case ExistingInstall::Comparison::UpdateAvailable:
        return Installer::Existing::UpdateAvailable;
    case ExistingInstall::Comparison::UpToDate:
        return Installer::Existing::UpToDate;
    case ExistingInstall::Comparison::Unknown:
        return Installer::Existing::Unknown;
    }
    return Installer::Existing::Unknown;
}
}

Installer::Installer(QObject *parent)
    : QObject(parent)
    , m_flathub(new FlathubClient(this))
    , m_icons(new IconDownloader(this))
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

bool Installer::downloadsExtras() const
{
    return m_downloadsExtras;
}

Installer::Existing Installer::existing() const
{
    return m_comparison;
}

bool Installer::isExistingForEveryone() const
{
    return m_existing && m_existing->forEveryone;
}

QString Installer::existingVersion() const
{
    return m_existing ? m_existing->version : QString();
}

bool Installer::canChangeExisting() const
{
    // m_systemAvailable was worked out for the change this would make.
    return m_existing && (!m_existing->forEveryone || m_systemAvailable);
}

bool Installer::offersChange() const
{
    return canChangeExisting() && m_comparison != Existing::UpToDate;
}

Installer::Outcome Installer::outcome() const
{
    return m_outcome;
}

QString Installer::sourceTitle() const
{
    return PermissionText::sourceTitle(m_package);
}

QString Installer::sourceExplanation() const
{
    return PermissionText::sourceExplanation(m_package);
}

bool Installer::sourceNeedsTrust() const
{
    return PermissionText::sourceNeedsTrust(m_package);
}

bool Installer::permissionsKnown() const
{
    return !m_package.metadata.isEmpty();
}

QVariantList Installer::permissions() const
{
    return m_permissions;
}

void Installer::updatePermissions()
{
    m_permissions.clear();
    const QList<Permissions::Permission> permissions = Permissions::fromMetadata(m_package.metadata);
    for (const Permissions::Permission &permission : permissions) {
        m_permissions.append(QVariantMap{
            {u"text"_s, PermissionText::describe(permission)},
            {u"needsTrust"_s, Permissions::needsTrust(permission.kind)},
        });
    }
    Q_EMIT packageChanged();
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
    m_permissions.clear();
    m_path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    m_hasSystemInstallation = false;
    m_userState = {};
    m_systemState = {};
    m_systemAvailable = false;
    m_installedForEveryone = false;
    m_downloadsExtras = false;
    m_existing.reset();
    m_comparison = Existing::Unknown;
    m_outcome = Outcome::Installed;

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
    updatePermissions();
    Q_EMIT packageChanged();
    fetchIcon();
    setState(State::Checking);
    checkInstallations();
}

// A .flatpakref only links to its icon. It arrives while the checks run;
// until then the generic icon is shown.
void Installer::fetchIcon()
{
    const QUrl url(m_package.iconUrl);
    if (!m_package.icon.isNull() || !IconDownloader::isAllowed(url, QUrl(m_package.remoteUrl))) {
        return;
    }
    const quint64 generation = m_generation;
    m_icons->fetch(url, [this, generation](const QImage &icon) {
        if (generation == m_generation && !icon.isNull()) {
            m_package.icon = icon;
            Q_EMIT packageChanged();
        }
    });
}

// The checks below run one after another on the host, then end in Ready:
// installations → runtime → remotes → polkit authorizations.

void Installer::checkInstallations()
{
    runOnHost(u"flatpak"_s, {u"--installations"_s}, [this](const Output &installations) {
        if (!installations.ranNormally || installations.exitCode == kCommandNotFound) {
            fail(State::FlatpakMissing,
                 i18n("This computer can't install Flatpak apps yet, because the Flatpak software isn't installed."),
                 QString::fromUtf8(installations.standardError).trimmed());
            return;
        }
        m_hasSystemInstallation = installations.exitCode == 0 && !installations.standardOutput.trimmed().isEmpty();
        checkRuntime();
    });
}

void Installer::checkRuntime()
{
    if (m_package.format != PackageInfo::Format::Bundle || m_package.runtime.isEmpty()) {
        checkRemotes();
        return;
    }
    runOnHost(u"flatpak"_s, FlatpakCli::runtimeInfoArguments(m_package.runtime, false), [this](const Output &user) {
        const bool inUser = user.ranNormally && user.exitCode == 0;
        if (!m_hasSystemInstallation) {
            m_userState.runtimeInstalled = inUser;
            checkRemotes();
            return;
        }
        runOnHost(u"flatpak"_s, FlatpakCli::runtimeInfoArguments(m_package.runtime, true), [this, inUser](const Output &system) {
            const bool inSystem = system.ranNormally && system.exitCode == 0;
            // Apps installed for one user can use runtimes installed system-wide.
            m_userState.runtimeInstalled = inUser || inSystem;
            m_systemState.runtimeInstalled = inSystem;
            checkRemotes();
        });
    });
}

void Installer::checkRemotes()
{
    auto apply = [this](HostEnvironment::InstallationState &state, const Output &remotes) {
        const QByteArray list = remotes.ranNormally && remotes.exitCode == 0 ? remotes.standardOutput : QByteArray();
        state.flathubConfigured = !HostEnvironment::remoteNameForUrl(FlatpakCli::flathubUrl(), list).isEmpty();
        state.remoteName = HostEnvironment::remoteNameForUrl(m_package.remoteUrl, list);
        state.remoteConfigured = !state.remoteName.isEmpty();
    };
    runOnHost(u"flatpak"_s, FlatpakCli::remotesArguments(false), [this, apply](const Output &user) {
        apply(m_userState, user);
        if (!m_hasSystemInstallation) {
            checkInstalled();
            return;
        }
        runOnHost(u"flatpak"_s, FlatpakCli::remotesArguments(true), [this, apply](const Output &system) {
            apply(m_systemState, system);
            checkInstalled();
        });
    });
}

// Is this app already installed? Look in the user's installation first,
// which is also the one `flatpak run` prefers.
void Installer::checkInstalled()
{
    runOnHost(u"flatpak"_s, FlatpakCli::listArguments(m_package.kind, false), [this](const Output &user) {
        const QByteArray list = user.ranNormally && user.exitCode == 0 ? user.standardOutput : QByteArray();
        std::optional<ExistingInstall::Installed> found = ExistingInstall::findInList(m_package, list, false);
        if (found || !m_hasSystemInstallation) {
            checkLatestCommit(found);
            return;
        }
        runOnHost(u"flatpak"_s, FlatpakCli::listArguments(m_package.kind, true), [this](const Output &system) {
            const QByteArray list = system.ranNormally && system.exitCode == 0 ? system.standardOutput : QByteArray();
            checkLatestCommit(ExistingInstall::findInList(m_package, list, true));
        });
    });
}

// For a .flatpakref, ask the remote for its newest build, to tell whether
// the installed copy is up to date.
void Installer::checkLatestCommit(std::optional<ExistingInstall::Installed> found)
{
    m_existing = found;
    const auto compare = [this](const QString &latestCommit) {
        if (m_existing) {
            m_comparison = toExisting(ExistingInstall::compare(m_package, *m_existing, latestCommit));
        }
        checkRefMetadata();
    };
    const QString remote = found ? (found->forEveryone ? m_systemState : m_userState).remoteName : QString();
    if (!found || m_package.format != PackageInfo::Format::Ref || remote.isEmpty()) {
        compare({});
        return;
    }
    runOnHost(u"flatpak"_s, FlatpakCli::remoteCommitArguments(remote, m_package, found->forEveryone), [compare](const Output &info) {
        compare(info.ranNormally && info.exitCode == 0 ? QString::fromUtf8(info.standardOutput).trimmed() : QString());
    });
}

// A .flatpakref doesn't say what the app may access. If its remote is
// already set up somewhere, ask that remote for the app's metadata;
// otherwise, for Flathub apps, ask Flathub's website.
void Installer::checkRefMetadata()
{
    if (m_package.format != PackageInfo::Format::Ref) {
        finishChecks();
        return;
    }
    const bool fromSystem = m_systemState.remoteConfigured;
    const QString remote = fromSystem ? m_systemState.remoteName : m_userState.remoteName;
    if (remote.isEmpty()) {
        checkFlathubMetadata();
        return;
    }
    runOnHost(u"flatpak"_s, FlatpakCli::remoteInfoArguments(remote, m_package, fromSystem), [this](const Output &info) {
        if (info.ranNormally && info.exitCode == 0) {
            m_package.metadata = info.standardOutput;
            updatePermissions();
            finishChecks();
            return;
        }
        checkFlathubMetadata();
    });
}

void Installer::checkFlathubMetadata()
{
    if (Permissions::sourceOf(m_package) != Permissions::Source::Flathub) {
        finishChecks();
        return;
    }
    const quint64 generation = m_generation;
    m_flathub->fetchMetadata(m_package, [this, generation](const QByteArray &metadata) {
        if (generation != m_generation) {
            return;
        }
        if (!metadata.isEmpty()) {
            m_package.metadata = metadata;
            updatePermissions();
        }
        finishChecks();
    });
}

void Installer::finishChecks()
{
    QStringList actions;
    if (m_hasSystemInstallation) {
        // A system-wide copy from a remote is updated, not installed.
        const bool updatesSystemCopy = m_existing && m_existing->forEveryone && m_package.format == PackageInfo::Format::Ref;
        actions = updatesSystemCopy ? HostEnvironment::systemUpdateActions(m_package) : HostEnvironment::systemInstallActions(m_package, m_systemState);
    }
    checkAuthorizations(actions, {});
}

void Installer::checkAuthorizations(QStringList actions, QList<HostEnvironment::Authorization> results)
{
    if (actions.isEmpty()) {
        const bool steamOS = HostEnvironment::isSteamOS(HostEnvironment::readHostOsRelease(HostCommand::runningInFlatpak()));
        m_systemAvailable = HostEnvironment::offerSystemInstall(m_hasSystemInstallation, results, steamOS);
        setState(m_existing ? State::AlreadyInstalled : State::Ready);
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
    m_outcome = Outcome::Installed;
    startInstall(forEveryone, false);
}

void Installer::changeExisting()
{
    if (m_state != State::AlreadyInstalled || !offersChange()) {
        return;
    }
    const bool forEveryone = m_existing->forEveryone;
    if (m_package.format == PackageInfo::Format::Ref) {
        // A .flatpakref can't be reinstalled from; update from its remote.
        m_outcome = m_comparison == Existing::UpdateAvailable ? Outcome::Updated : Outcome::UpToDate;
        m_installedForEveryone = forEveryone;
        m_downloadsExtras = false;
        setState(State::Installing);
        runOnHost(u"flatpak"_s, FlatpakCli::updateArguments(m_package.id, m_existing->branch, forEveryone), [this](const Output &output) {
            finishInstall(output);
        });
        return;
    }
    m_outcome = m_comparison == Existing::FileIsNewer ? Outcome::Updated : Outcome::Installed;
    startInstall(forEveryone, true);
}

void Installer::startInstall(bool forEveryone, bool reinstall)
{
    const HostEnvironment::InstallationState &target = forEveryone ? m_systemState : m_userState;
    m_installedForEveryone = forEveryone;
    m_downloadsExtras = !target.runtimeInstalled;
    setState(State::Installing);

    if (!HostEnvironment::needsFlathub(m_package, target)) {
        runInstall(forEveryone, reinstall);
        return;
    }
    const QStringList addFlathub = FlatpakCli::addRemoteArguments(FlatpakCli::flathubRemoteName(), FlatpakCli::flathubRepoFile(), forEveryone);
    runOnHost(u"flatpak"_s, addFlathub, [this, forEveryone, reinstall](const Output &output) {
        if (output.ranNormally && output.exitCode == 0) {
            runInstall(forEveryone, reinstall);
            return;
        }
        failInstall(output);
    });
}

void Installer::runInstall(bool forEveryone, bool reinstall)
{
    runOnHost(u"flatpak"_s, FlatpakCli::installArguments(m_package, m_path, forEveryone, reinstall), [this](const Output &output) {
        finishInstall(output);
    });
}

void Installer::finishInstall(const Output &output)
{
    if (output.ranNormally && output.exitCode == 0) {
        setState(State::Installed);
        return;
    }
    failInstall(output);
}

void Installer::openExisting()
{
    if (m_state != State::AlreadyInstalled || !m_existing || !canLaunch()) {
        return;
    }
    PackageInfo installed = m_package;
    installed.branch = m_existing->branch;
    QProcess *process = HostCommand::create(u"flatpak"_s, FlatpakCli::runArguments(installed, m_existing->forEveryone), nullptr);
    process->startDetached();
    delete process;
}

void Installer::failInstall(const Output &output)
{
    const QString details = QString::fromUtf8(output.standardError + output.standardOutput).trimmed();
    fail(State::Failed,
         m_outcome == Outcome::Installed ? i18n("“%1” couldn't be installed.", m_package.displayName()) : i18n("“%1” couldn't be updated.", m_package.displayName()),
         details);
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
        setState(m_existing ? State::AlreadyInstalled : State::Ready);
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
