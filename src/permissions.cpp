// SPDX-License-Identifier: Apache-2.0
#include "permissions.h"

#include <QSet>
#include <QUrl>

#include <glib.h>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Permissions
{

namespace
{

QStringList listValue(GKeyFile *keyFile, const char *group, const char *key)
{
    gsize length = 0;
    g_auto(GStrv) values = g_key_file_get_string_list(keyFile, group, key, &length, nullptr);
    QStringList list;
    for (gsize i = 0; i < length; ++i) {
        const QString value = QString::fromUtf8(values[i]).trimmed();
        // "!name" takes a permission away; an empty entry is a trailing ';'.
        if (!value.isEmpty() && !value.startsWith(u'!')) {
            list.append(value);
        }
    }
    return list;
}

bool isUserFolder(const QString &path)
{
    static const QStringList folders{
        u"xdg-desktop"_s,
        u"xdg-documents"_s,
        u"xdg-download"_s,
        u"xdg-music"_s,
        u"xdg-pictures"_s,
        u"xdg-public-share"_s,
        u"xdg-templates"_s,
        u"xdg-videos"_s,
    };
    const QString base = path.section(u'/', 0, 0);
    return folders.contains(base);
}

bool isAppDataFolder(const QString &path)
{
    const QString base = path.section(u'/', 0, 0);
    return base == "xdg-config"_L1 || base == "xdg-data"_L1 || base == "xdg-cache"_L1 || base == "xdg-run"_L1 || base == "xdg-state"_L1;
}

class Collector
{
public:
    void add(Kind kind, bool readOnly = false)
    {
        // Read-write wins over read-only for the same kind.
        for (Permission &existing : m_permissions) {
            if (existing.kind == kind && existing.kind != Kind::Folders) {
                existing.readOnly = existing.readOnly && readOnly;
                return;
            }
        }
        m_permissions.append({.kind = kind, .readOnly = readOnly, .folders = {}});
    }

    void addFilesystem(const QString &entry)
    {
        QString path = entry;
        bool readOnly = false;
        if (const qsizetype colon = path.lastIndexOf(u':'); colon > 0) {
            readOnly = path.mid(colon + 1) == "ro"_L1;
            path = path.left(colon);
        }
        while (path.size() > 1 && path.endsWith(u'/')) {
            path.chop(1);
        }

        if (path == "host"_L1) {
            add(Kind::AllFiles, readOnly);
        } else if (path == "host-os"_L1 || path == "host-etc"_L1 || path == "host-root"_L1) {
            add(Kind::SystemFiles, readOnly);
        } else if (path == "home"_L1 || path == "~"_L1) {
            add(Kind::HomeFolder, readOnly);
        } else if (isAppDataFolder(path)) {
            // Reading e.g. xdg-config/kdeglobals for the colour scheme is common
            // and harmless; only writing is worth mentioning.
            if (!readOnly) {
                add(Kind::OtherAppsData);
            }
        } else {
            (readOnly ? m_readOnlyFolders : m_folders).append(path);
        }
    }

    QList<Permission> result()
    {
        const auto has = [this](Kind kind, bool writable) {
            return std::ranges::any_of(m_permissions, [&](const Permission &p) {
                return p.kind == kind && (!writable || !p.readOnly);
            });
        };
        // Access to everything makes single folders redundant.
        const bool allFiles = has(Kind::AllFiles, true);
        if (allFiles) {
            m_permissions.removeIf([](const Permission &p) {
                return p.kind == Kind::HomeFolder;
            });
        }
        const bool wholeHome = allFiles || has(Kind::HomeFolder, true);
        const bool wholeHomeReadable = wholeHome || has(Kind::HomeFolder, false) || has(Kind::AllFiles, false);
        const auto inHome = [](const QString &path) {
            return isUserFolder(path) || path.startsWith("~/"_L1);
        };
        if (wholeHome) {
            m_folders.removeIf(inHome);
        }
        if (wholeHomeReadable) {
            m_readOnlyFolders.removeIf(inHome);
        }
        for (const QString &folder : std::as_const(m_folders)) {
            m_readOnlyFolders.removeAll(folder);
        }
        m_folders.removeDuplicates();
        m_readOnlyFolders.removeDuplicates();
        if (!m_folders.isEmpty()) {
            m_permissions.append({.kind = Kind::Folders, .readOnly = false, .folders = m_folders});
        }
        if (!m_readOnlyFolders.isEmpty()) {
            m_permissions.append({.kind = Kind::Folders, .readOnly = true, .folders = m_readOnlyFolders});
        }
        std::ranges::stable_sort(m_permissions, {}, &Permission::kind);
        return m_permissions;
    }

private:
    QList<Permission> m_permissions;
    QStringList m_folders;
    QStringList m_readOnlyFolders;
};

} // namespace

Source sourceOf(const PackageInfo &package)
{
    if (package.format == PackageInfo::Format::Bundle) {
        return Source::File;
    }
    const QString host = QUrl(package.remoteUrl).host().toLower();
    return host == "dl.flathub.org"_L1 || host == "flathub.org"_L1 ? Source::Flathub : Source::OtherSite;
}

bool needsTrust(Kind kind)
{
    return kind < Kind::Folders;
}

QList<Permission> fromMetadata(const QByteArray &metadata)
{
    g_autoptr(GKeyFile) keyFile = g_key_file_new();
    if (!g_key_file_load_from_data(keyFile, metadata.constData(), metadata.size(), G_KEY_FILE_NONE, nullptr)) {
        return {};
    }

    Collector collector;
    constexpr auto context = "Context";

    if (listValue(keyFile, context, "shared").contains("network"_L1)) {
        collector.add(Kind::Network);
    }

    const QStringList sockets = listValue(keyFile, context, "sockets");
    const QList<std::pair<QString, Kind>> socketKinds{
        {u"x11"_s, Kind::LegacyDisplay},
        {u"pulseaudio"_s, Kind::Sound},
        {u"cups"_s, Kind::Printing},
        {u"pcsc"_s, Kind::SmartCards},
        {u"ssh-auth"_s, Kind::SshKeys},
        {u"gpg-agent"_s, Kind::EncryptionKeys},
        {u"session-bus"_s, Kind::AllAppsBus},
        {u"system-bus"_s, Kind::SystemBus},
    };
    for (const auto &[socket, kind] : socketKinds) {
        if (sockets.contains(socket)) {
            collector.add(kind);
        }
    }

    const QStringList devices = listValue(keyFile, context, "devices");
    const QList<std::pair<QString, Kind>> deviceKinds{
        {u"all"_s, Kind::AllDevices},
        {u"input"_s, Kind::GameControllers},
        {u"usb"_s, Kind::UsbDevices},
        {u"kvm"_s, Kind::Virtualization},
    };
    for (const auto &[device, kind] : deviceKinds) {
        if (devices.contains(device)) {
            collector.add(kind);
        }
    }

    if (listValue(keyFile, context, "features").contains("bluetooth"_L1)) {
        collector.add(Kind::Bluetooth);
    }

    const QStringList filesystems = listValue(keyFile, context, "filesystems");
    for (const QString &entry : filesystems) {
        collector.addFilesystem(entry);
    }

    static const QSet<QString> passwordServices{
        u"org.freedesktop.secrets"_s,
        u"org.gnome.keyring"_s,
        u"org.kde.kwalletd5"_s,
        u"org.kde.kwalletd6"_s,
    };
    constexpr auto sessionBus = "Session Bus Policy";
    g_auto(GStrv) names = g_key_file_get_keys(keyFile, sessionBus, nullptr, nullptr);
    for (gchar **name = names; name && *name; ++name) {
        g_autofree gchar *policy = g_key_file_get_string(keyFile, sessionBus, *name, nullptr);
        if (g_strcmp0(policy, "none") == 0) {
            continue;
        }
        const QString service = QString::fromUtf8(*name);
        if (service == "org.freedesktop.Flatpak"_L1) {
            collector.add(Kind::RunAnyProgram);
        } else if (passwordServices.contains(service)) {
            collector.add(Kind::Passwords);
        }
    }

    return collector.result();
}

} // namespace Permissions
