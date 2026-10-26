// SPDX-License-Identifier: Apache-2.0
#include "bundlecontents.h"

#include <KCompressionDevice>

#include <QBuffer>
#include <QDir>

#include <glib.h>
#include <sys/stat.h>

using namespace Qt::StringLiterals;

namespace
{

// See ostree-repo-static-delta-private.h and ostree-core.h in libostree.
constexpr int kSuperblockMetadataIndex = 0;
constexpr int kSuperblockCommitIndex = 4;
constexpr int kSuperblockPartsIndex = 6;
constexpr int kPartHeaderObjectsIndex = 4;
constexpr int kCommitRootTreeIndex = 6;
constexpr auto kPartType = "(a(uuu)aa(ayay)ayay)";
constexpr auto kDirTreeType = "(a(say)a(sayay))";

constexpr qsizetype kChecksumSize = 32;
constexpr qsizetype kObjectNameSize = 1 + kChecksumSize; // type, checksum
constexpr char kFileObject = 1;
constexpr char kDirTreeObject = 2;

// Parts are at most 32 MB unless one file is bigger than that.
constexpr qint64 kMaxPartSize = 512 * 1024 * 1024;
constexpr int kMaxSymlinks = 8;

QByteArray fixedArray(GVariant *variant)
{
    gsize size = 0;
    const auto *data = static_cast<const char *>(g_variant_get_fixed_array(variant, &size, 1));
    return QByteArray(data, static_cast<qsizetype>(size));
}

QByteArray objectName(char type, const QByteArray &checksum)
{
    return type + checksum;
}

bool isMetadataObject(char type)
{
    return type != kFileObject;
}

QByteArray decompress(const char *data, qsizetype size, char compression)
{
    if (compression == 0) {
        return QByteArray(data, size);
    }
    if (compression != 'x') {
        return {};
    }
    QBuffer buffer;
    buffer.setData(QByteArray::fromRawData(data, size));
    KCompressionDevice device(&buffer, false, KCompressionDevice::Xz);
    if (!device.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray result;
    constexpr qint64 chunk = 1024 * 1024;
    while (!device.atEnd() && result.size() <= kMaxPartSize) {
        const QByteArray bytes = device.read(chunk);
        if (bytes.isEmpty()) {
            break;
        }
        result += bytes;
    }
    return result.size() <= kMaxPartSize ? result : QByteArray();
}

// Reads a protobuf-style varint, as written by _ostree_write_varuint64.
std::optional<quint64> readVarint(const char *&data, const char *end)
{
    quint64 value = 0;
    for (int shift = 0; shift < 64 && data < end; shift += 7) {
        const auto byte = static_cast<quint8>(*data++);
        value |= quint64(byte & 0x7f) << shift;
        if (!(byte & 0x80)) {
            return value;
        }
    }
    return std::nullopt;
}

// The checksum of the entry called `name` in a dirtree's list of files
// (index 0) or directories (index 1).
QByteArray findEntry(GVariant *tree, int listIndex, const QString &name)
{
    const QByteArray wanted = name.toUtf8();
    g_autoptr(GVariant) list = g_variant_get_child_value(tree, listIndex);
    const gsize count = g_variant_n_children(list);
    for (gsize i = 0; i < count; ++i) {
        g_autoptr(GVariant) entry = g_variant_get_child_value(list, i);
        g_autoptr(GVariant) entryName = g_variant_get_child_value(entry, 0);
        if (qstrcmp(g_variant_get_string(entryName, nullptr), wanted.constData()) == 0) {
            g_autoptr(GVariant) checksum = g_variant_get_child_value(entry, 1);
            return fixedArray(checksum);
        }
    }
    return {};
}

void deleteByteArray(gpointer data)
{
    delete static_cast<QByteArray *>(data);
}

} // namespace

BundleContents::BundleContents(GVariant *delta)
{
    g_autoptr(GVariant) commit = g_variant_get_child_value(delta, kSuperblockCommitIndex);
    g_autoptr(GVariant) rootTree = g_variant_get_child_value(commit, kCommitRootTreeIndex);
    m_rootTree = fixedArray(rootTree);

    g_autoptr(GVariant) headers = g_variant_get_child_value(delta, kSuperblockPartsIndex);
    const gsize partCount = g_variant_n_children(headers);
    m_parts.resize(static_cast<qsizetype>(partCount));
    for (gsize i = 0; i < partCount; ++i) {
        g_autoptr(GVariant) header = g_variant_get_child_value(headers, i);
        g_autoptr(GVariant) objects = g_variant_get_child_value(header, kPartHeaderObjectsIndex);
        Part &part = m_parts[static_cast<qsizetype>(i)];
        part.objects = fixedArray(objects);
        for (qsizetype offset = 0; offset + kObjectNameSize <= part.objects.size(); offset += kObjectNameSize) {
            m_partOf.insert(part.objects.mid(offset, kObjectNameSize), static_cast<int>(i));
        }
    }

    // A bundle keeps its parts in the superblock's metadata, under the
    // paths they would have in a repository: "deltas/<to>/<index>".
    g_autoptr(GVariant) metadata = g_variant_get_child_value(delta, kSuperblockMetadataIndex);
    GVariantIter iter;
    g_variant_iter_init(&iter, metadata);
    const gchar *key = nullptr;
    GVariant *value = nullptr;
    while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
        bool isIndex = false;
        const QString path = QString::fromUtf8(key);
        const uint index = path.section(u'/', -1).toUInt(&isIndex);
        if (path.startsWith("deltas/"_L1) && isIndex && index < partCount && g_variant_is_of_type(value, G_VARIANT_TYPE("(yay)"))) {
            std::swap(m_parts[index].packed, value);
        }
        g_clear_pointer(&value, g_variant_unref);
    }
}

BundleContents::~BundleContents()
{
    for (Part &part : m_parts) {
        g_clear_pointer(&part.packed, g_variant_unref);
        g_clear_pointer(&part.unpacked, g_variant_unref);
    }
}

bool BundleContents::unpack(int index)
{
    Part &part = m_parts[index];
    if (part.unpacked || part.failed) {
        return part.unpacked;
    }
    part.failed = true;
    if (!part.packed) {
        return false;
    }

    guint8 compression = 0;
    g_autoptr(GVariant) packedData = nullptr;
    g_variant_get(part.packed, "(y@ay)", &compression, &packedData);
    gsize packedSize = 0;
    const auto *packed = static_cast<const char *>(g_variant_get_fixed_array(packedData, &packedSize, 1));
    auto *data = new QByteArray(decompress(packed, static_cast<qsizetype>(packedSize), static_cast<char>(compression)));
    if (data->isEmpty()) {
        delete data;
        return false;
    }
    g_autoptr(GBytes) bytes = g_bytes_new_with_free_func(data->constData(), data->size(), deleteByteArray, data);
    part.unpacked = g_variant_ref_sink(g_variant_new_from_bytes(G_VARIANT_TYPE(kPartType), bytes, FALSE));

    g_autoptr(GVariant) modes = g_variant_get_child_value(part.unpacked, 0);
    g_autoptr(GVariant) payload = g_variant_get_child_value(part.unpacked, 2);
    g_autoptr(GVariant) operations = g_variant_get_child_value(part.unpacked, 3);
    gsize payloadSize = 0;
    // Points into part.unpacked, which outlives the child variant.
    part.payload = static_cast<const char *>(g_variant_get_fixed_array(payload, &payloadSize, 1));
    part.payloadSize = static_cast<qsizetype>(payloadSize);
    gsize operationsSize = 0;
    const auto *op = static_cast<const char *>(g_variant_get_fixed_array(operations, &operationsSize, 1));
    const char *end = op + operationsSize;

    const qsizetype objectCount = part.objects.size() / kObjectNameSize;
    qsizetype objectIndex = 0;
    QByteArray openName; // object between "open" and "close"
    Object open;

    auto next = [&]() {
        return readVarint(op, end);
    };
    auto slice = [&](std::optional<quint64> length, std::optional<quint64> offset) -> std::optional<Slice> {
        if (!length || !offset || *offset > quint64(part.payloadSize) || *length > quint64(part.payloadSize) - *offset) {
            return std::nullopt;
        }
        return Slice{.offset = qsizetype(*offset), .length = qsizetype(*length)};
    };
    auto modeAt = [&](std::optional<quint64> index) -> std::optional<quint32> {
        if (!index || *index >= g_variant_n_children(modes)) {
            return std::nullopt;
        }
        g_autoptr(GVariant) entry = g_variant_get_child_value(modes, *index);
        guint32 uid = 0;
        guint32 gid = 0;
        guint32 mode = 0;
        g_variant_get(entry, "(uuu)", &uid, &gid, &mode);
        return GUINT32_FROM_BE(mode);
    };

    while (op < end) {
        switch (*op++) {
        case 'S': { // open, splice and close
            if (objectIndex >= objectCount) {
                return false;
            }
            const QByteArray name = part.objects.mid(objectIndex * kObjectNameSize, kObjectNameSize);
            Object object{.part = index, .mode = 0, .slices = {}};
            if (!isMetadataObject(name[0])) {
                const auto mode = modeAt(next());
                next(); // xattrs
                if (!mode) {
                    return false;
                }
                object.mode = *mode;
            }
            const auto length = next();
            const auto data = slice(length, next());
            if (!data) {
                return false;
            }
            object.slices.append(*data);
            m_objects.insert(name, object);
            ++objectIndex;
            break;
        }
        case 'o': { // open
            if (objectIndex >= objectCount || !openName.isEmpty()) {
                return false;
            }
            const auto mode = modeAt(next());
            next(); // xattrs
            if (!mode || !next()) { // total size
                return false;
            }
            openName = part.objects.mid(objectIndex * kObjectNameSize, kObjectNameSize);
            open = Object{.part = index, .mode = *mode, .slices = {}};
            break;
        }
        case 'w': { // write
            const auto length = next();
            const auto data = slice(length, next());
            if (openName.isEmpty() || !data) {
                return false;
            }
            open.slices.append(*data);
            break;
        }
        case 'c': // close
            if (openName.isEmpty()) {
                return false;
            }
            m_objects.insert(openName, open);
            openName.clear();
            ++objectIndex;
            break;
        default:
            // Setting a read source and applying a binary patch need an
            // older commit to start from, which a bundle never has.
            return false;
        }
    }

    part.failed = false;
    return true;
}

const BundleContents::Object *BundleContents::object(const QByteArray &name)
{
    auto found = m_objects.constFind(name);
    if (found == m_objects.constEnd()) {
        const int part = m_partOf.value(name, -1);
        if (part < 0 || !unpack(part)) {
            return nullptr;
        }
        found = m_objects.constFind(name);
    }
    return found == m_objects.constEnd() ? nullptr : &*found;
}

QByteArray BundleContents::objectData(const Object &object) const
{
    const Part &part = m_parts[object.part];
    QByteArray data;
    for (const Slice &slice : object.slices) {
        data.append(part.payload + slice.offset, slice.length);
    }
    return data;
}

GVariant *BundleContents::dirTree(const QString &path)
{
    QByteArray checksum = m_rootTree;
    const QStringList names = path.split(u'/', Qt::SkipEmptyParts);
    for (qsizetype i = 0;; ++i) {
        const Object *tree = checksum.size() == kChecksumSize ? object(objectName(kDirTreeObject, checksum)) : nullptr;
        if (!tree) {
            return nullptr;
        }
        const QByteArray data = objectData(*tree);
        g_autoptr(GBytes) bytes = g_bytes_new(data.constData(), data.size());
        GVariant *variant = g_variant_ref_sink(g_variant_new_from_bytes(G_VARIANT_TYPE(kDirTreeType), bytes, FALSE));
        if (i == names.size()) {
            return variant;
        }
        checksum = findEntry(variant, 1, names[i]);
        g_variant_unref(variant);
    }
}

std::optional<QByteArray> BundleContents::readFile(const QString &path)
{
    QString current = QDir::cleanPath(path);
    for (int links = 0; links <= kMaxSymlinks; ++links) {
        if (current.isEmpty() || current.startsWith(u'/') || current.startsWith(".."_L1)) {
            return std::nullopt;
        }
        const QString dir = current.section(u'/', 0, -2);
        g_autoptr(GVariant) tree = dirTree(dir);
        if (!tree) {
            return std::nullopt;
        }
        const QByteArray checksum = findEntry(tree, 0, current.section(u'/', -1));
        const Object *file = checksum.size() == kChecksumSize ? object(objectName(kFileObject, checksum)) : nullptr;
        if (!file) {
            return std::nullopt;
        }
        if (S_ISREG(file->mode)) {
            return objectData(*file);
        }
        if (!S_ISLNK(file->mode)) {
            return std::nullopt;
        }
        const QString target = QString::fromUtf8(objectData(*file));
        if (target.startsWith("/app/"_L1)) {
            // Exports often point back into the app's files.
            current = QDir::cleanPath(u"files/"_s + target.mid(5));
        } else {
            current = QDir::cleanPath(dir.isEmpty() ? target : dir + u'/' + target);
        }
    }
    return std::nullopt;
}

QStringList BundleContents::subdirectories(const QString &path)
{
    QStringList names;
    g_autoptr(GVariant) tree = dirTree(path);
    if (!tree) {
        return names;
    }
    g_autoptr(GVariant) dirs = g_variant_get_child_value(tree, 1);
    const gsize count = g_variant_n_children(dirs);
    for (gsize i = 0; i < count; ++i) {
        g_autoptr(GVariant) entry = g_variant_get_child_value(dirs, i);
        g_autoptr(GVariant) name = g_variant_get_child_value(entry, 0);
        names.append(QString::fromUtf8(g_variant_get_string(name, nullptr)));
    }
    return names;
}
