// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QImage>
#include <QObject>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;

// Downloads the icon a .flatpakref links to in its Icon= key, so it can be
// shown before installing.
class IconDownloader : public QObject
{
    Q_OBJECT

public:
    explicit IconDownloader(QObject *parent);

    // Calls `done` with the icon, or with a null image when it can't be
    // downloaded or read. Gives up after a few seconds.
    void fetch(const QUrl &url, std::function<void(const QImage &icon)> done);

    // Only icons from the repository's own website are downloaded, over
    // HTTPS: opening a file shouldn't tell anyone else about it.
    static bool isAllowed(const QUrl &icon, const QUrl &repository);

private:
    QNetworkAccessManager *m_network;
};
