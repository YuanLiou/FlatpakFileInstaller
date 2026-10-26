// SPDX-License-Identifier: Apache-2.0
#include "packagefile.h"

#include <KCompressionDevice>

#include <QBuffer>
#include <QFile>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QtEndian>

#include <glib.h>

using namespace Qt::StringLiterals;

QString PackageInfo::displayName() const
{
    if (!name.isEmpty()) {
        return name;
    }
    return id.section(u'.', -1);
}

namespace
{

// A .flatpak bundle is an OSTree static delta superblock serialized as a
// GVariant; see OSTREE_STATIC_DELTA_SUPERBLOCK_FORMAT in libostree.
constexpr auto kBundleType = "(a{sv}tayay(a{sv}aya(say)sstayay)aya(uayttay)a(yaytt))";
constexpr int kBundleMetadataIndex = 0;
constexpr int kBundleCommitIndex = 4;

constexpr qsizetype kMaxRefFileSize = 1024 * 1024;
constexpr qint64 kMaxAppdataSize = 4 * 1024 * 1024;

bool isValidId(const QString &id)
{
    static const QRegularExpression re(u"^[A-Za-z_][A-Za-z0-9_-]*(\\.[A-Za-z_][A-Za-z0-9_-]*){2,}$"_s);
    return id.size() <= 255 && re.match(id).hasMatch();
}

bool isValidBranch(const QString &branch)
{
    static const QRegularExpression re(u"^[A-Za-z0-9_.-]+$"_s);
    return re.match(branch).hasMatch();
}

QString lookupString(GVariant *dict, const char *key)
{
    const gchar *value = nullptr;
    if (g_variant_lookup(dict, key, "&s", &value)) {
        return QString::fromUtf8(value);
    }
    return {};
}

QByteArray lookupBytes(GVariant *dict, const char *key)
{
    g_autoptr(GVariant) value = g_variant_lookup_value(dict, key, G_VARIANT_TYPE_BYTESTRING);
    if (!value) {
        return {};
    }
    gsize size = 0;
    const auto *data = static_cast<const char *>(g_variant_get_fixed_array(value, &size, 1));
    return QByteArray(data, static_cast<qsizetype>(size));
}

QString normalizedLanguage(QString lang)
{
    return lang.replace(u'-', u'_');
}

// "zh-Hant-TW", "zh-TW" and "zh_TW" all become "zh_TW".
QString canonicalLanguage(const QString &tag)
{
    const QLocale locale(tag);
    return locale.language() == QLocale::C ? QString() : locale.name();
}

// Fills name, summary, developer and version from the bundle's gzipped
// AppStream data. Only the first <component> is used.
void applyAppdata(PackageInfo &info, const QByteArray &gzipped, const QStringList &languages)
{
    QBuffer buffer;
    buffer.setData(gzipped);
    KCompressionDevice device(&buffer, false, KCompressionDevice::GZip);
    if (!device.open(QIODevice::ReadOnly)) {
        return;
    }
    QXmlStreamReader xml(device.read(kMaxAppdataSize));

    QHash<QString, QString> names;
    QHash<QString, QString> summaries;
    QStringList stack;
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::EndElement) {
            stack.removeLast();
            if (xml.name() == "component"_L1) {
                break;
            }
            continue;
        }
        if (token != QXmlStreamReader::StartElement) {
            continue;
        }

        const QString element = xml.name().toString();
        const QString parent = stack.isEmpty() ? QString() : stack.last();
        const QString lang = normalizedLanguage(xml.attributes().value("xml:lang"_L1).toString());

        if (parent == "component"_L1 && element == "name"_L1) {
            names.insert(lang, xml.readElementText().simplified());
        } else if (parent == "component"_L1 && element == "summary"_L1) {
            summaries.insert(lang, xml.readElementText().simplified());
        } else if (parent == "component"_L1 && element == "developer_name"_L1 && lang.isEmpty()) {
            info.developer = xml.readElementText().simplified();
        } else if (parent == "developer"_L1 && element == "name"_L1 && lang.isEmpty()) {
            info.developer = xml.readElementText().simplified();
        } else if (parent == "releases"_L1 && element == "release"_L1 && info.version.isEmpty()) {
            info.version = xml.attributes().value("version"_L1).toString();
            stack.append(element);
        } else {
            stack.append(element);
        }
        // readElementText() consumed the end tag, so nothing to pop.
    }

    if (const QString name = PackageFile::localizedValue(names, languages); !name.isEmpty()) {
        info.name = name;
    }
    info.summary = PackageFile::localizedValue(summaries, languages);
}

PackageFile::Result parseBundleBytes(GBytes *bytes, const QStringList &languages)
{
    g_autoptr(GVariant) delta = g_variant_ref_sink(g_variant_new_from_bytes(G_VARIANT_TYPE(kBundleType), bytes, FALSE));
    g_autoptr(GVariant) metadata = g_variant_get_child_value(delta, kBundleMetadataIndex);

    guchar endianness = 0;
    if (g_variant_lookup(metadata, "ostree.endianness", "y", &endianness)) {
        const bool swapped = (G_BYTE_ORDER == G_LITTLE_ENDIAN && endianness == 'B') || (G_BYTE_ORDER == G_BIG_ENDIAN && endianness == 'l');
        if (swapped) {
            g_autoptr(GVariant) native = g_variant_byteswap(delta);
            std::swap(delta, native);
            g_variant_unref(metadata);
            metadata = g_variant_get_child_value(delta, kBundleMetadataIndex);
        }
    }

    // ref is "app/<id>/<arch>/<branch>" or "runtime/<id>/<arch>/<branch>"
    const QStringList ref = lookupString(metadata, "ref").split(u'/');
    if (ref.size() != 4 || (ref[0] != "app"_L1 && ref[0] != "runtime"_L1) || !isValidId(ref[1]) || !isValidBranch(ref[3])) {
        return {.info = std::nullopt, .error = PackageFile::Error::NotFlatpak};
    }

    PackageInfo info;
    info.format = PackageInfo::Format::Bundle;
    info.kind = ref[0] == "app"_L1 ? PackageInfo::Kind::App : PackageInfo::Kind::Runtime;
    info.id = ref[1];
    info.arch = ref[2];
    info.branch = ref[3];
    info.remoteUrl = lookupString(metadata, "origin");
    info.runtimeRepoUrl = lookupString(metadata, "runtime-repo");

    const QByteArray appMetadata = lookupString(metadata, "metadata").toUtf8();
    info.metadata = appMetadata;
    g_autoptr(GKeyFile) keyFile = g_key_file_new();
    if (g_key_file_load_from_data(keyFile, appMetadata.constData(), appMetadata.size(), G_KEY_FILE_NONE, nullptr)) {
        g_autofree gchar *runtime = g_key_file_get_string(keyFile, "Application", "runtime", nullptr);
        info.runtime = QString::fromUtf8(runtime);
    }

    if (const QByteArray appdata = lookupBytes(metadata, "appdata"); !appdata.isEmpty()) {
        applyAppdata(info, appdata, languages);
    }

    for (const char *key : {"icon-128", "icon-64"}) {
        if (const QByteArray png = lookupBytes(metadata, key); !png.isEmpty()) {
            info.icon = QImage::fromData(png, "PNG");
            if (!info.icon.isNull()) {
                break;
            }
        }
    }

    g_autoptr(GVariant) commit = g_variant_get_child_value(delta, kBundleCommitIndex);
    g_autoptr(GVariant) commitMetadata = g_variant_get_child_value(commit, 0);
    guint64 installedSize = 0;
    if (g_variant_lookup(commitMetadata, "xa.installed-size", "t", &installedSize)) {
        info.installedSize = qFromBigEndian<quint64>(installedSize);
    }

    return {.info = std::move(info), .error = PackageFile::Error::None};
}

bool looksLikeRef(const QByteArray &data)
{
    return data.size() <= kMaxRefFileSize && data.contains("[Flatpak Ref]");
}

} // namespace

namespace PackageFile
{

bool isInstallableFileName(const QString &fileName)
{
    return fileName.endsWith(".flatpak"_L1, Qt::CaseInsensitive) || fileName.endsWith(".flatpakref"_L1, Qt::CaseInsensitive);
}

QString localizedValue(const QHash<QString, QString> &values, const QStringList &languages)
{
    for (const QString &lang : languages) {
        const auto exact = values.constFind(normalizedLanguage(lang));
        if (exact != values.constEnd()) {
            return *exact;
        }
        // Only compare regional tags this way: a bare "zh" would otherwise
        // match whichever Chinese variant QLocale picks as its default.
        if (!lang.contains(u'_') && !lang.contains(u'-')) {
            continue;
        }
        const QString wanted = canonicalLanguage(lang);
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            if (!it.key().isEmpty() && canonicalLanguage(it.key()) == wanted) {
                return it.value();
            }
        }
    }
    return values.value(QString());
}

Result parseBundle(const QByteArray &data, const QStringList &languages)
{
    g_autoptr(GBytes) bytes = g_bytes_new(data.constData(), data.size());
    return parseBundleBytes(bytes, languages);
}

Result parseRef(const QByteArray &data)
{
    constexpr auto group = "Flatpak Ref";
    g_autoptr(GKeyFile) keyFile = g_key_file_new();
    if (!g_key_file_load_from_data(keyFile, data.constData(), data.size(), G_KEY_FILE_NONE, nullptr)
        || !g_key_file_has_group(keyFile, group)) {
        return {.info = std::nullopt, .error = Error::NotFlatpak};
    }

    auto get = [&](const char *key) {
        g_autofree gchar *value = g_key_file_get_string(keyFile, group, key, nullptr);
        return QString::fromUtf8(value).trimmed();
    };

    PackageInfo info;
    info.format = PackageInfo::Format::Ref;
    info.id = get("Name");
    info.branch = get("Branch");
    info.remoteUrl = get("Url");
    if (!isValidId(info.id) || info.remoteUrl.isEmpty() || (!info.branch.isEmpty() && !isValidBranch(info.branch))) {
        return {.info = std::nullopt, .error = Error::NotFlatpak};
    }
    info.kind = g_key_file_get_boolean(keyFile, group, "IsRuntime", nullptr) ? PackageInfo::Kind::Runtime : PackageInfo::Kind::App;
    info.name = get("Title");
    info.summary = get("Comment");
    info.runtimeRepoUrl = get("RuntimeRepo");
    return {.info = std::move(info), .error = Error::None};
}

Result load(const QString &path, const QStringList &languages)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {.info = std::nullopt, .error = Error::Unreadable};
    }

    const QByteArray head = file.peek(kMaxRefFileSize + 1);
    if (looksLikeRef(head)) {
        return parseRef(head);
    }

    // Bundles can be hundreds of megabytes: map instead of reading.
    g_autoptr(GMappedFile) mapped = g_mapped_file_new(QFile::encodeName(path).constData(), FALSE, nullptr);
    if (!mapped) {
        return {.info = std::nullopt, .error = Error::Unreadable};
    }
    g_autoptr(GBytes) bytes = g_mapped_file_get_bytes(mapped);
    return parseBundleBytes(bytes, languages);
}

} // namespace PackageFile
