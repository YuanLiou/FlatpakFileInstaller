// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

// Answers every request on a local port with one canned HTTP response.
class FakeServer : public QTcpServer
{
public:
    QByteArray status = "200 OK";
    QByteArray contentType = "application/json";
    QByteArray body;
    QByteArray requestLine;

    FakeServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                if (requestLine.isEmpty()) {
                    requestLine = socket->readLine().trimmed();
                }
                socket->readAll();
                socket->write("HTTP/1.1 " + status + "\r\nContent-Type: " + contentType + "\r\nContent-Length: " + QByteArray::number(body.size())
                              + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        listen(QHostAddress::LocalHost);
    }

    QUrl base() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/api/v2/").arg(serverPort()));
    }
};
