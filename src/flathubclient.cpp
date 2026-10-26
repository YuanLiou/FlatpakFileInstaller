// SPDX-License-Identifier: Apache-2.0
#include "flathubclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

#include <glib.h>

using namespace Qt::StringLiterals;

namespace
{
constexpr int kTimeoutMs = 8000;
constexpr qint64 kMaxReplySize = 1024 * 1024;

void setList(GKeyFile *keyFile, const char *group, const QString &key, const QJsonArray &values)
{
    QList<QByteArray> items;
    for (const QJsonValue &value : values) {
        if (value.isString()) {
            items.append(value.toString().toUtf8());
        }
    }
    QList<const char *> pointers;
    for (const QByteArray &item : std::as_const(items)) {
        pointers.append(item.constData());
    }
    g_key_file_set_string_list(keyFile, group, key.toUtf8().constData(), pointers.constData(), pointers.size());
}

bool isBusName(const QString &name)
{
    static const QRegularExpression re(u"^[A-Za-z0-9_.*-]{1,255}$"_s);
    return re.match(name).hasMatch();
}

// {"talk": ["org.example.A"], "own": [...]} → name=talk lines.
void setBusPolicy(GKeyFile *keyFile, const char *group, const QJsonObject &policies)
{
    for (auto it = policies.constBegin(); it != policies.constEnd(); ++it) {
        const QByteArray policy = it.key().toUtf8();
        const QJsonArray names = it.value().toArray();
        for (const QJsonValue &name : names) {
            if (name.isString() && isBusName(name.toString())) {
                g_key_file_set_string(keyFile, group, name.toString().toUtf8().constData(), policy.constData());
            }
        }
    }
}
} // namespace

FlathubClient::FlathubClient(QObject *parent, const QUrl &apiBase)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_apiBase(apiBase)
{
}

QUrl FlathubClient::summaryUrl(const PackageInfo &package) const
{
    // The ID was validated when the .flatpakref was read, so it is safe in a path.
    QUrl url = m_apiBase.resolved(QUrl(u"summary/"_s + package.id));
    if (!package.branch.isEmpty()) {
        url.setQuery(QUrlQuery{{u"branch"_s, package.branch}});
    }
    return url;
}

void FlathubClient::fetchMetadata(const PackageInfo &package, std::function<void(const QByteArray &)> done)
{
    QNetworkRequest request(summaryUrl(package));
    request.setTransferTimeout(kTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, u"flatpak-file-installer"_s);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done({});
            return;
        }
        done(metadataFromSummary(reply->read(kMaxReplySize)));
    });
}

QByteArray FlathubClient::metadataFromSummary(const QByteArray &json)
{
    const QJsonObject metadata = QJsonDocument::fromJson(json).object().value("metadata"_L1).toObject();
    if (!metadata.contains("permissions"_L1) || !metadata.value("permissions"_L1).isObject()) {
        return {};
    }
    const QJsonObject permissions = metadata.value("permissions"_L1).toObject();

    // Only the keys Permissions reads, and GKeyFile does the escaping, so
    // nothing in the reply can add groups or keys of its own.
    static const QStringList contextKeys{u"shared"_s, u"sockets"_s, u"devices"_s, u"features"_s, u"filesystems"_s};
    g_autoptr(GKeyFile) keyFile = g_key_file_new();
    g_key_file_set_string(keyFile, "Application", "name", metadata.value("name"_L1).toString().toUtf8().constData());
    for (auto it = permissions.constBegin(); it != permissions.constEnd(); ++it) {
        if (it.key() == "session-bus"_L1) {
            setBusPolicy(keyFile, "Session Bus Policy", it.value().toObject());
        } else if (it.key() == "system-bus"_L1) {
            setBusPolicy(keyFile, "System Bus Policy", it.value().toObject());
        } else if (contextKeys.contains(it.key()) && it.value().isArray()) {
            setList(keyFile, "Context", it.key(), it.value().toArray());
        }
    }
    gsize length = 0;
    g_autofree gchar *data = g_key_file_to_data(keyFile, &length, nullptr);
    return QByteArray(data, static_cast<qsizetype>(length));
}
