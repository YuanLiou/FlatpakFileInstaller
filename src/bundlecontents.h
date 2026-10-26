// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QStringList>

#include <optional>

using GVariant = struct _GVariant;

// Reads single files out of a .flatpak bundle without installing it.
//
// A bundle is an OSTree static delta made from scratch: its parts hold every
// object of the commit, usually xz-compressed, with a list of operations
// that write the objects out. A part is only unpacked when something in it
// is read, so reading a few small files stays cheap even for large apps.
//
// Objects are not checked against their checksums. This is only meant for
// things shown before installing (an icon, a name), which the bundle could
// state about itself anyway; flatpak checks everything when installing.
class BundleContents
{
public:
    // `delta` is the bundle's superblock, in host byte order.
    explicit BundleContents(GVariant *delta);
    ~BundleContents();
    BundleContents(const BundleContents &) = delete;
    BundleContents &operator=(const BundleContents &) = delete;

    // `path` is relative to the commit's root, such as
    // "export/share/applications/org.example.App.desktop". Follows symlinks
    // that stay inside the commit.
    std::optional<QByteArray> readFile(const QString &path);

    // Names of the directories inside the directory at `path`.
    QStringList subdirectories(const QString &path);

private:
    struct Slice {
        qsizetype offset = 0;
        qsizetype length = 0;
    };
    struct Object {
        int part = -1;
        quint32 mode = 0; // content objects only
        QList<Slice> slices; // in the part's payload
    };
    struct Part {
        GVariant *packed = nullptr; // (yay): compression type, data
        GVariant *unpacked = nullptr; // OSTREE_STATIC_DELTA_PART_PAYLOAD_FORMAT_V0
        const char *payload = nullptr;
        qsizetype payloadSize = 0;
        QByteArray objects; // object names, as listed in the superblock
        bool failed = false;
    };

    bool unpack(int index);
    const Object *object(const QByteArray &name);
    QByteArray objectData(const Object &object) const;
    GVariant *dirTree(const QString &path);

    QList<Part> m_parts;
    QHash<QByteArray, int> m_partOf; // object name -> part index
    QHash<QByteArray, Object> m_objects; // objects of unpacked parts
    QByteArray m_rootTree; // checksum of the commit's root directory
};
