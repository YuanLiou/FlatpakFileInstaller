// SPDX-License-Identifier: Apache-2.0
#include "preferences.h"

#include "installer.h"

#include <KLocalizedString>

#include <QFile>
#include <QJSEngine>
#include <QQmlEngine>

#include <unistd.h>

using namespace Qt::StringLiterals;

namespace
{
Preferences *s_instance = nullptr;
}

Preferences::Preferences(const Startup &startup, Installer *installer, QObject *parent)
    : QObject(parent)
    , m_installer(installer)
    , m_startup(startup)
    , m_settings(LanguagePreference::openSettings())
    , m_language(LanguagePreference::storedChoice(*m_settings, startup.available))
{
}

Preferences *Preferences::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_ASSERT(s_instance);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void Preferences::setInstance(Preferences *instance)
{
    s_instance = instance;
}

QString Preferences::languageOverride(const QStringList &available)
{
    const auto settings = LanguagePreference::openSettings();
    return LanguagePreference::storedChoice(*settings, available);
}

QVariantList Preferences::languages() const
{
    QVariantList list;
    list.append(QVariantMap{{u"code"_s, QString()}, {u"name"_s, i18n("Follow System")}});
    for (const QString &code : m_startup.available) {
        list.append(QVariantMap{{u"code"_s, code}, {u"name"_s, LanguagePreference::nativeName(code)}});
    }
    return list;
}

QString Preferences::language() const
{
    return m_language;
}

// The prompt is written in the language that was just chosen, so that
// someone who picked it can read it.
QString Preferences::restartTitle() const
{
    return ki18n("Restart to switch to %1").subs(LanguagePreference::nativeName(m_target)).toString(QStringList{m_target});
}

QString Preferences::restartExplanation() const
{
    return ki18n("Your choice is saved. The new language is used the next time you open this app.").toString(QStringList{m_target});
}

QString Preferences::restartNowText() const
{
    return ki18n("Restart").toString(QStringList{m_target});
}

QString Preferences::restartLaterText() const
{
    return ki18n("Restart Later").toString(QStringList{m_target});
}

void Preferences::chooseLanguage(const QString &code)
{
    const QString choice = code.isEmpty() || m_startup.available.contains(code) ? code : QString();
    if (choice == m_language) {
        return;
    }
    LanguagePreference::storeChoice(*m_settings, choice);
    m_language = choice;
    Q_EMIT languageChanged();

    m_target = LanguagePreference::effectiveLanguage(choice, m_startup.systemLanguages, m_startup.available);
    if (m_target != m_startup.language) {
        Q_EMIT restartPrompted();
    }
}

bool Preferences::restart()
{
    if (m_installer->isBusy()) {
        return false;
    }

    const QStringList arguments = LanguagePreference::restartArguments(m_startup.arguments, m_startup.files, m_installer->restartFile());
    QList<QByteArray> encoded;
    for (const QString &argument : arguments) {
        encoded.append(QFile::encodeName(argument));
    }
    QList<char *> argv;
    for (QByteArray &argument : encoded) {
        argv.append(argument.data());
    }
    argv.append(nullptr);

    // Replace this process instead of starting another one: in a Flatpak
    // the sandbox goes away with its first process, and the portal's path
    // to the opened file is only valid inside it. The new process should
    // start from the environment we started with, not from our override.
    LanguagePreference::SavedEnvironment current;
    for (const QByteArray &name : m_startup.environment.names()) {
        current.remember(name);
    }
    // exec's file name becomes the process name ("exe" for the symlink), so
    // use the file it points to, if that is still there.
    const QByteArray self = "/proc/self/exe";
    const QString target = QFile::symLinkTarget(QString::fromLatin1(self));
    const QByteArray program = !target.isEmpty() && QFile::exists(target) ? QFile::encodeName(target) : self;
    m_startup.environment.restore();
    execv(program.constData(), argv.data());
    // Still here: couldn't restart. Carry on with what we had.
    current.restore();
    return false;
}
