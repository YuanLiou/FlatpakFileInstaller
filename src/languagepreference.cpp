// SPDX-License-Identifier: Apache-2.0
#include "languagepreference.h"

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QStandardPaths>
#include <algorithm>

#include <locale.h>

using namespace Qt::StringLiterals;

namespace LanguagePreference
{

namespace
{
const QString kEnglish = u"en"_s;
const QString kChoiceKey = u"language"_s;

// "zh-Hant-TW", "zh_TW.UTF-8@x" -> language, script, territory in the
// spelling that catalog directories use ("zh_TW"). Parts that are missing
// stay empty.
struct Parts {
    QString language;
    QString script;
    QString territory;
};

Parts split(const QString &locale)
{
    QString name = locale.left(locale.indexOf(u'.') < 0 ? locale.size() : locale.indexOf(u'.'));
    name = name.left(name.indexOf(u'@') < 0 ? name.size() : name.indexOf(u'@'));
    name.replace(u'-', u'_');

    Parts parts;
    const QStringList pieces = name.split(u'_', Qt::SkipEmptyParts);
    if (pieces.isEmpty()) {
        return parts;
    }
    parts.language = pieces.first().toLower();
    for (qsizetype i = 1; i < pieces.size(); ++i) {
        if (pieces.at(i).size() == 4) {
            parts.script = pieces.at(i).toLower();
        } else {
            parts.territory = pieces.at(i).toUpper();
        }
    }
    return parts;
}
}

QStringList availableCodes(const QSet<QString> &translations)
{
    QStringList codes;
    for (const QString &code : translations) {
        // en_US is the language of the source strings, KI18n lists it too.
        if (!code.isEmpty() && code != kEnglish && code != u"en_US"_s) {
            codes.append(code);
        }
    }
    std::sort(codes.begin(), codes.end());
    codes.prepend(kEnglish);
    return codes;
}

QStringList availableCodes(const QString &domain)
{
    // The same folders KI18n looks in. KLocalizedString::availableDomainTranslations()
    // does this too, but it makes Qt and KI18n read the language from the
    // environment right then, before LANGUAGE has been set from the saved choice.
    QSet<QString> translations;
    const QStringList localeDirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, u"locale"_s, QStandardPaths::LocateDirectory);
    for (const QString &localeDir : localeDirs) {
        const QStringList languages = QDir(localeDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &language : languages) {
            if (QFile::exists(localeDir + u'/' + language + u"/LC_MESSAGES/"_s + domain + u".mo"_s)) {
                translations.insert(language);
            }
        }
    }
    return availableCodes(translations);
}

QString nativeName(const QString &code)
{
    // QLocale("en") is American English, whose name in itself is
    // "American English".
    QString name = code == kEnglish ? u"English"_s : QLocale(code).nativeLanguageName();
    if (name.isEmpty()) {
        return code;
    }
    // Some languages write their name in lower case ("français").
    return name.at(0).toUpper() + name.mid(1);
}

QString match(const QString &locale, const QStringList &available)
{
    Parts parts = split(locale);
    if (parts.language.isEmpty() || parts.language == u"c" || parts.language == u"posix") {
        return {};
    }
    // Chinese is written in a script; without a territory, that tells which.
    if (parts.language == u"zh" && parts.territory.isEmpty()) {
        if (parts.script == u"hant") {
            parts.territory = u"TW"_s;
        } else if (parts.script == u"hans") {
            parts.territory = u"CN"_s;
        }
    }

    const QString full = parts.territory.isEmpty() ? parts.language : parts.language + u'_' + parts.territory;
    for (const QString &code : available) {
        if (code.compare(full, Qt::CaseInsensitive) == 0) {
            return code;
        }
    }
    // "en_US" or "pt_BR" can use a translation of the bare language.
    if (!parts.territory.isEmpty()) {
        for (const QString &code : available) {
            if (code.compare(parts.language, Qt::CaseInsensitive) == 0) {
                return code;
            }
        }
    }
    return {};
}

QString resolve(const QStringList &locales, const QStringList &available)
{
    for (const QString &locale : locales) {
        if (const QString code = match(locale, available); !code.isEmpty()) {
            return code;
        }
    }
    return kEnglish;
}

bool localeExists(const QString &name)
{
    const locale_t locale = newlocale(LC_MESSAGES_MASK, QFile::encodeName(name).constData(), nullptr);
    if (!locale) {
        return false;
    }
    freelocale(locale);
    return true;
}

QString messagesLocale(const QString &lcAll, const QString &lcMessages, const QString &lang)
{
    for (const QString &value : {lcAll, lcMessages, lang}) {
        if (!value.isEmpty()) {
            return value;
        }
    }
    return u"C"_s;
}

bool isUsableLocale(const QString &name, const LocaleExists &exists)
{
    // "C.UTF-8" counts as "C" here.
    const QString language = split(name).language;
    if (language.isEmpty() || language == u"c" || language == u"posix") {
        return false;
    }
    return exists(name);
}

QStringList systemLanguages(const QString &language, const QString &lcAll, const QString &lcMessages, const QString &lang, const LocaleExists &exists)
{
    const QString locale = messagesLocale(lcAll, lcMessages, lang);
    if (!isUsableLocale(locale, exists)) {
        return {};
    }
    QStringList result = language.split(u':', Qt::SkipEmptyParts);
    result.append(locale);
    return result;
}

QStringList systemLanguages()
{
    return systemLanguages(qEnvironmentVariable("LANGUAGE"),
                           qEnvironmentVariable("LC_ALL"),
                           qEnvironmentVariable("LC_MESSAGES"),
                           qEnvironmentVariable("LANG"),
                           localeExists);
}

QString replacementLocale(const QString &code, const QString &lcAll, const QString &lcMessages, const QString &lang, const LocaleExists &exists)
{
    // English is what untranslated text is, whatever the locale.
    if (code.isEmpty() || code == kEnglish || isUsableLocale(messagesLocale(lcAll, lcMessages, lang), exists)) {
        return {};
    }
    for (const QString &candidate : QStringList{code + u".UTF-8"_s, u"en_US.UTF-8"_s}) {
        if (exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

void applyChoice(const QString &code, const LocaleExists &exists)
{
    if (code.isEmpty()) {
        return;
    }
    const QString lcAll = qEnvironmentVariable("LC_ALL");
    const QString locale = replacementLocale(code, lcAll, qEnvironmentVariable("LC_MESSAGES"), qEnvironmentVariable("LANG"), exists);
    if (!locale.isEmpty()) {
        // LC_ALL beats LC_MESSAGES, so it has to be the one that changes.
        qputenv(lcAll.isEmpty() ? "LC_MESSAGES" : "LC_ALL", locale.toUtf8());
    }
    // KI18n stops looking at "en_US" (the language of the source text), but
    // not at plain "en": the next language of the system would then be used.
    // After "zh_TW" it also keeps strings that aren't translated in English.
    qputenv("LANGUAGE", code == kEnglish ? QByteArrayLiteral("en_US") : code.toUtf8() + ":en_US");
}

std::unique_ptr<QSettings> openSettings()
{
    return std::make_unique<QSettings>(u"yuanliou"_s, u"flatpak-file-installer"_s);
}

QString storedChoice(QSettings &settings, const QStringList &available)
{
    const QString code = settings.value(kChoiceKey).toString();
    return available.contains(code) ? code : QString();
}

void storeChoice(QSettings &settings, const QString &code)
{
    if (code.isEmpty()) {
        settings.remove(kChoiceKey);
    } else {
        settings.setValue(kChoiceKey, code);
    }
    settings.sync();
}

QString effectiveLanguage(const QString &choice, const QStringList &systemLanguages, const QStringList &available)
{
    if (!choice.isEmpty() && available.contains(choice)) {
        return choice;
    }
    return resolve(systemLanguages, available);
}

QStringList restartArguments(const QStringList &arguments, const QStringList &files, const QString &file)
{
    QStringList result = arguments;
    for (const QString &opened : files) {
        result.removeOne(opened);
    }
    if (result.size() > 1 && result.last() == u"--") {
        result.removeLast(); // it only introduced the file we removed
    }
    if (!file.isEmpty()) {
        // After "--", so that a name starting with "-" is still a file.
        result << u"--"_s << file;
    }
    return result;
}

void SavedEnvironment::remember(const QByteArray &name)
{
    if (m_values.contains(name)) {
        return;
    }
    m_values.insert(name, qEnvironmentVariableIsSet(name.constData()) ? std::optional(qgetenv(name.constData())) : std::nullopt);
}

void SavedEnvironment::restore() const
{
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it) {
        if (it.value()) {
            qputenv(it.key().constData(), *it.value());
        } else {
            qunsetenv(it.key().constData());
        }
    }
}

void SavedEnvironment::applyTo(QProcessEnvironment &env) const
{
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it) {
        const QString name = QString::fromLatin1(it.key());
        if (it.value()) {
            env.insert(name, QString::fromLocal8Bit(*it.value()));
        } else {
            env.remove(name);
        }
    }
}

QList<QByteArray> SavedEnvironment::names() const
{
    return m_values.keys();
}

} // namespace LanguagePreference
