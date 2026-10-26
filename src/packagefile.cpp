// SPDX-License-Identifier: Apache-2.0
#include "packagefile.h"

#include "bundlecontents.h"

#include <KCompressionDevice>

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QImageReader>
#include <QLocale>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QtEndian>

#include <glib.h>

#include <algorithm>

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
// Big enough for the largest icon size the interface shows, on a 2x screen.
constexpr int kIconSize = 256;

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

// Values of `key` and its localized forms ("Name", "Name[zh_TW]", ...) from
// the [Desktop Entry] group.
QHash<QString, QString> desktopValues(GKeyFile *keyFile, const QString &key)
{
    QHash<QString, QString> values;
    constexpr auto group = "Desktop Entry";
    g_auto(GStrv) keys = g_key_file_get_keys(keyFile, group, nullptr, nullptr);
    for (gchar **it = keys; it && *it; ++it) {
        const QString name = QString::fromUtf8(*it);
        QString lang;
        if (name.startsWith(key + u'[') && name.endsWith(u']')) {
            lang = normalizedLanguage(name.mid(key.size() + 1).chopped(1));
        } else if (name != key) {
            continue;
        }
        g_autofree gchar *value = g_key_file_get_string(keyFile, group, *it, nullptr);
        values.insert(lang, QString::fromUtf8(value).simplified());
    }
    return values;
}

QImage readIcon(const QByteArray &data, const char *format)
{
    QBuffer buffer;
    buffer.setData(data);
    QImageReader reader(&buffer, format);
    const QSize size = reader.size();
    // SVGs are drawn at whatever size is asked for; big PNGs are scaled down.
    const bool scalable = qstrncmp(format, "svg", 3) == 0;
    if (size.isValid() && (scalable || size.width() > kIconSize || size.height() > kIconSize)) {
        reader.setScaledSize(size.scaled(kIconSize, kIconSize, Qt::KeepAspectRatio));
    }
    return reader.read();
}

// The app's icon as exported for menus: a scalable one if there is one
// (and Qt can draw it), otherwise the biggest PNG.
QImage exportedIcon(BundleContents &contents, const QString &name)
{
    const QString base = u"export/share/icons/hicolor"_s;
    for (const char *format : {"svg", "svgz"}) {
        if (const auto data = contents.readFile(u"%1/scalable/apps/%2.%3"_s.arg(base, name, QLatin1StringView(format)))) {
            if (QImage icon = readIcon(*data, format); !icon.isNull()) {
                return icon;
            }
        }
    }

    static const QRegularExpression sizeDir(u"^(\\d+)x\\1$"_s);
    QList<int> sizes;
    for (const QString &dir : contents.subdirectories(base)) {
        if (const auto match = sizeDir.match(dir); match.hasMatch()) {
            sizes.append(match.captured(1).toInt());
        }
    }
    std::sort(sizes.begin(), sizes.end(), std::greater<>());
    for (const int size : std::as_const(sizes)) {
        if (const auto data = contents.readFile(u"%1/%2x%2/apps/%3.png"_s.arg(base).arg(size).arg(name))) {
            if (QImage icon = readIcon(*data, "png"); !icon.isNull()) {
                return icon;
            }
        }
    }
    return {};
}

// Bundles built without AppStream data carry no icon or name of their own.
// The desktop file and icons the app exports for menus are still inside.
void applyExports(PackageInfo &info, GVariant *delta, const QStringList &languages)
{
    BundleContents contents(delta);
    QString iconName = info.id;
    if (const auto desktop = contents.readFile(u"export/share/applications/%1.desktop"_s.arg(info.id))) {
        g_autoptr(GKeyFile) keyFile = g_key_file_new();
        if (g_key_file_load_from_data(keyFile, desktop->constData(), desktop->size(), G_KEY_FILE_KEEP_TRANSLATIONS, nullptr)) {
            if (info.name.isEmpty()) {
                info.name = PackageFile::localizedValue(desktopValues(keyFile, u"Name"_s), languages);
            }
            if (info.summary.isEmpty()) {
                info.summary = PackageFile::localizedValue(desktopValues(keyFile, u"Comment"_s), languages);
            }
            // Exported icons are named after the app ID, but may add to it
            // (org.example.App.Devel).
            const QString icon = desktopValues(keyFile, u"Icon"_s).value(QString());
            if (icon.startsWith(info.id) && !icon.contains(u'/')) {
                iconName = icon;
            }
        }
    }
    if (info.icon.isNull()) {
        info.icon = exportedIcon(contents, iconName);
    }
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

    if (info.kind == PackageInfo::Kind::App && (info.icon.isNull() || info.name.isEmpty())) {
        applyExports(info, delta, languages);
    }

    g_autoptr(GVariant) commit = g_variant_get_child_value(delta, kBundleCommitIndex);
    // An OSTree commit's ID is the SHA-256 of its serialized GVariant.
    if (g_autoptr(GBytes) commitBytes = g_variant_get_data_as_bytes(commit)) {
        gsize size = 0;
        const auto *data = static_cast<const char *>(g_bytes_get_data(commitBytes, &size));
        info.commit = QString::fromLatin1(QCryptographicHash::hash(QByteArrayView(data, static_cast<qsizetype>(size)), QCryptographicHash::Sha256).toHex());
    }
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
    info.iconUrl = get("Icon");
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
