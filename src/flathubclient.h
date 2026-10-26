// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "packageinfo.h"

#include <QByteArray>
#include <QObject>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;

// Asks Flathub's website API for an app's permissions, for a .flatpakref
// whose Flathub remote isn't set up on this computer yet (so
// `flatpak remote-info` can't be used).
class FlathubClient : public QObject
{
    Q_OBJECT

public:
    // `apiBase` is overridable for tests.
    explicit FlathubClient(QObject *parent, const QUrl &apiBase = QUrl(QStringLiteral("https://flathub.org/api/v2/")));

    // Calls `done` with the app's metadata in Flatpak's key file format, or
    // with an empty array when Flathub doesn't know the app or can't be
    // reached. Gives up after a few seconds.
    void fetchMetadata(const PackageInfo &package, std::function<void(const QByteArray &metadata)> done);

    QUrl summaryUrl(const PackageInfo &package) const;

    // Turns the JSON of /summary/<app-id> back into a Flatpak metadata key
    // file, so that Permissions::fromMetadata() can read it. Empty when
    // the JSON has no permission information.
    static QByteArray metadataFromSummary(const QByteArray &json);

private:
    QNetworkAccessManager *m_network;
    QUrl m_apiBase;
};
