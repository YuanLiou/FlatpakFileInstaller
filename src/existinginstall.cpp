// SPDX-License-Identifier: Apache-2.0
#include "existinginstall.h"

#include <QStringList>

using namespace Qt::StringLiterals;

namespace ExistingInstall
{

namespace
{

// Commits are compared by prefix: `flatpak list` shortens them.
bool sameCommit(const QString &a, const QString &b)
{
    if (a.size() < 12 || b.size() < 12) {
        return false;
    }
    return a.startsWith(b, Qt::CaseInsensitive) || b.startsWith(a, Qt::CaseInsensitive);
}

QStringList versionSegments(const QString &version)
{
    QStringList segments;
    QString current;
    bool currentIsDigit = false;
    for (const QChar c : version) {
        if (!c.isLetterOrNumber()) {
            if (!current.isEmpty()) {
                segments.append(current);
                current.clear();
            }
            continue;
        }
        if (!current.isEmpty() && c.isDigit() != currentIsDigit) {
            segments.append(current);
            current.clear();
        }
        currentIsDigit = c.isDigit();
        current.append(c);
    }
    if (!current.isEmpty()) {
        segments.append(current);
    }
    return segments;
}

int compareNumbers(QString a, QString b)
{
    while (a.size() > 1 && a.startsWith(u'0')) {
        a.remove(0, 1);
    }
    while (b.size() > 1 && b.startsWith(u'0')) {
        b.remove(0, 1);
    }
    if (a.size() != b.size()) {
        return a.size() < b.size() ? -1 : 1;
    }
    return a.compare(b);
}

} // namespace

std::optional<Installed> findInList(const PackageInfo &package, const QByteArray &listOutput, bool forEveryone)
{
    const QStringList lines = QString::fromUtf8(listOutput).split(u'\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList columns = line.split(u'\t');
        if (columns.size() < 5) {
            continue;
        }
        const QString &id = columns.at(0);
        const QString &branch = columns.at(1);
        const QString &arch = columns.at(2);
        if (id != package.id || (!package.branch.isEmpty() && branch != package.branch) || (!package.arch.isEmpty() && arch != package.arch)) {
            continue;
        }
        return Installed{
            .forEveryone = forEveryone,
            .branch = branch,
            .version = columns.at(3).trimmed(),
            .commit = columns.at(4).trimmed(),
        };
    }
    return std::nullopt;
}

Comparison compare(const PackageInfo &package, const Installed &installed, const QString &latestCommit)
{
    if (package.format == PackageInfo::Format::Ref) {
        if (latestCommit.isEmpty()) {
            return Comparison::Unknown;
        }
        return sameCommit(latestCommit, installed.commit) ? Comparison::UpToDate : Comparison::UpdateAvailable;
    }

    if (sameCommit(package.commit, installed.commit)) {
        return Comparison::SameVersion;
    }
    if (package.version.isEmpty() || installed.version.isEmpty()) {
        return Comparison::DifferentVersion;
    }
    const int order = compareVersions(package.version, installed.version);
    if (order > 0) {
        return Comparison::FileIsNewer;
    }
    if (order < 0) {
        return Comparison::FileIsOlder;
    }
    // Same version number, different build: still "the same version" to a person.
    return Comparison::SameVersion;
}

int compareVersions(const QString &a, const QString &b)
{
    const QStringList left = versionSegments(a);
    const QStringList right = versionSegments(b);
    for (qsizetype i = 0; i < qMin(left.size(), right.size()); ++i) {
        const bool leftDigit = left.at(i).front().isDigit();
        const bool rightDigit = right.at(i).front().isDigit();
        int order = 0;
        if (leftDigit && rightDigit) {
            order = compareNumbers(left.at(i), right.at(i));
        } else if (leftDigit != rightDigit) {
            // A number beats letters: 1.2 is newer than 1.beta.
            order = leftDigit ? 1 : -1;
        } else {
            order = left.at(i).compare(right.at(i), Qt::CaseInsensitive);
        }
        if (order != 0) {
            return order < 0 ? -1 : 1;
        }
    }
    if (left.size() == right.size()) {
        return 0;
    }
    return left.size() < right.size() ? -1 : 1;
}

} // namespace ExistingInstall
