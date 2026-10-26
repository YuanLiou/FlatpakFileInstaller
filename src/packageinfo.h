// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

// What we know about a .flatpak / .flatpakref file before installing it.
struct PackageInfo {
    enum class Format {
        Bundle, // .flatpak: the whole app in one file
        Ref, // .flatpakref: points to a remote repository
    };
    enum class Kind {
        App,
        Runtime,
    };

    Format format = Format::Bundle;
    Kind kind = Kind::App;

    QString id; // e.g. org.kde.kate
    QString arch; // empty for .flatpakref (resolved by flatpak on install)
    QString branch;

    QString name; // human readable, localized when the file provides it
    QString summary;
    QString version;
    QString developer;
    QImage icon;
    QString iconUrl; // .flatpakref: where its icon can be downloaded

    QString runtime; // e.g. org.kde.Platform/x86_64/6.10 (bundles only)
    QString runtimeRepoUrl; // .flatpakrepo that provides the runtime
    QString remoteUrl; // .flatpakref: repository URL; bundle: origin URL
    quint64 installedSize = 0; // bytes, 0 when unknown
    QString commit; // bundles: the OSTree commit checksum (hex)

    // The app's Flatpak metadata (permissions etc.). Bundles carry it;
    // for a .flatpakref it has to be fetched from the remote.
    QByteArray metadata;

    // Name to show; falls back to the last part of the ID.
    QString displayName() const;
};
