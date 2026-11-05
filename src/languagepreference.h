// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

// Which language the interface uses: the languages that have a translation,
// the saved choice, and how to switch by restarting. Language codes look
// like "en" or "zh_TW"; an empty code means "follow the system".
//
// Nothing here needs a GUI, so it can be used before QApplication exists.
namespace LanguagePreference
{

// Languages with a translation, plus English (the source language), English
// first and the rest sorted by code. `translations` is the set of catalog
// directories found, e.g. {"zh_TW"}.
QStringList availableCodes(const QSet<QString> &translations);

// The same, looked up where KI18n finds `domain`'s catalogs: under "locale"
// in the XDG data directories.
QStringList availableCodes(const QString &domain);

// The language's name in itself, e.g. "English" or "繁體中文".
QString nativeName(const QString &code);

// The entry of `available` that best matches a locale name such as "zh_TW",
// "zh-Hant-TW", "en_US.UTF-8" or "C", or an empty string if there is none.
QString match(const QString &locale, const QStringList &available);

// The language of the first entry of `locales` that has a translation, or
// "en" if none does.
QString resolve(const QStringList &locales, const QStringList &available);

// Whether the C library knows the locale `name` (e.g. "zh_TW.UTF-8").
bool localeExists(const QString &name);
using LocaleExists = std::function<bool(const QString &)>;

// The locale that picks the language of messages: LC_ALL, else LC_MESSAGES,
// else LANG, as the environment says.
QString messagesLocale(const QString &lcAll, const QString &lcMessages, const QString &lang);

// gettext, which KI18n uses, ignores LANGUAGE unless the locale is a real,
// installed one other than "C" (so not with LANG=C, nor with a locale that
// isn't generated on this computer).
bool isUsableLocale(const QString &name, const LocaleExists &exists);

// The languages the system asks for, most wanted first: LANGUAGE, then the
// locale. Nothing when gettext would ignore all of it.
QStringList systemLanguages(const QString &language, const QString &lcAll, const QString &lcMessages, const QString &lang, const LocaleExists &exists);
QStringList systemLanguages();

// Choosing a language only works through LANGUAGE if the locale is usable.
// When it isn't, this is a locale to use instead, found for `code`; it is
// empty when nothing needs changing or no usable locale exists.
QString replacementLocale(const QString &code, const QString &lcAll, const QString &lcMessages, const QString &lang, const LocaleExists &exists);

// Makes the process use the language `code` (not for an empty code): sets
// LANGUAGE, which KI18n, Kirigami and Qt read, and a replacement locale if
// the current one would make gettext ignore it. Call before QApplication.
void applyChoice(const QString &code, const LocaleExists &exists = localeExists);

// Where the choice is stored: ~/.config/yuanliou/flatpak-file-installer.conf,
// or below ~/.var/app/<id>/config in a Flatpak. The names are set
// explicitly because this is read before QApplication exists.
std::unique_ptr<QSettings> openSettings();

// The saved choice. A language that has no translation any more counts as
// "follow the system".
QString storedChoice(QSettings &settings, const QStringList &available);
void storeChoice(QSettings &settings, const QString &code);

// The language that `choice` leads to.
QString effectiveLanguage(const QString &choice, const QStringList &systemLanguages, const QStringList &available);

// Arguments for starting the app again: `arguments` (the first is the
// program name) without the files in `files`, then `file`, if any.
QStringList restartArguments(const QStringList &arguments, const QStringList &files, const QString &file);

// Environment variables as they were before we changed them, so that a
// restarted process starts from the same environment as this one did.
class SavedEnvironment
{
public:
    // Remembers the current value of `name` (unless already remembered).
    void remember(const QByteArray &name);
    // Puts every remembered variable back; unsets those that weren't set.
    void restore() const;
    QList<QByteArray> names() const;

private:
    QHash<QByteArray, std::optional<QByteArray>> m_values;
};

} // namespace LanguagePreference
