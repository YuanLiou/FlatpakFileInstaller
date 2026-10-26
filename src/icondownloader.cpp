// SPDX-License-Identifier: Apache-2.0
#include "icondownloader.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

using namespace Qt::StringLiterals;

namespace
{
constexpr int kTimeoutMs = 8000;
constexpr qint64 kMaxIconSize = 4 * 1024 * 1024;
// Big enough for the largest icon size the interface shows, on a 2x screen.
constexpr int kIconSize = 256;
} // namespace

IconDownloader::IconDownloader(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

bool IconDownloader::isAllowed(const QUrl &icon, const QUrl &repository)
{
    return icon.isValid() && icon.scheme() == "https"_L1 && !icon.host().isEmpty() && icon.host().compare(repository.host(), Qt::CaseInsensitive) == 0;
}

void IconDownloader::fetch(const QUrl &url, std::function<void(const QImage &)> done)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(kTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, u"flatpak-file-installer"_s);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received) {
        if (received > kMaxIconSize) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done({});
            return;
        }
        QImage icon = QImage::fromData(reply->read(kMaxIconSize));
        if (icon.width() > kIconSize || icon.height() > kIconSize) {
            icon = icon.scaled(kIconSize, kIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        done(icon);
    });
}
