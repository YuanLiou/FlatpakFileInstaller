// SPDX-License-Identifier: Apache-2.0
#include "fakeserver.h"
#include "flathubclient.h"
#include "permissions.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTest>

using namespace Qt::StringLiterals;
using Permissions::Kind;

namespace
{

// Shaped like https://flathub.org/api/v2/summary/<app-id>.
const QByteArray kSummary = R"({
  "arches": ["x86_64", "aarch64"],
  "branch": "stable",
  "installed_size": 1024,
  "metadata": {
    "name": "org.example.Notes",
    "runtime": "org.kde.Platform/x86_64/6.10",
    "permissions": {
      "shared": ["network", "ipc"],
      "sockets": ["wayland", "fallback-x11", "pulseaudio"],
      "devices": ["dri"],
      "filesystems": ["xdg-download", "xdg-config/kdeglobals:ro"],
      "session-bus": {"talk": ["org.freedesktop.secrets", "org.kde.StatusNotifierWatcher"]}
    }
  }
})";

QList<Kind> kinds(const QByteArray &metadata)
{
    QList<Kind> result;
    const QList<Permissions::Permission> permissions = Permissions::fromMetadata(metadata);
    for (const Permissions::Permission &permission : permissions) {
        result.append(permission.kind);
    }
    return result;
}

} // namespace

class FlathubClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void summaryToMetadata()
    {
        const QByteArray metadata = FlathubClient::metadataFromSummary(kSummary);
        QVERIFY(metadata.contains("[Context]"));
        QCOMPARE(kinds(metadata), (QList<Kind>{Kind::Passwords, Kind::Folders, Kind::Network, Kind::Sound}));
    }

    void noSpecialPermissions()
    {
        const QByteArray metadata = FlathubClient::metadataFromSummary(R"({"metadata": {"name": "org.sugarlabs.Maze", "permissions": {}}})");
        QVERIFY(!metadata.isEmpty());
        QVERIFY(Permissions::fromMetadata(metadata).isEmpty());
    }

    void unknownWithoutPermissions_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("no permissions key") << QByteArray(R"({"metadata": {"name": "org.example.App"}})");
        QTest::newRow("no metadata") << QByteArray(R"({"arches": ["x86_64"]})");
        QTest::newRow("not json") << QByteArray("<html>Not found</html>");
        QTest::newRow("empty") << QByteArray();
    }

    void unknownWithoutPermissions()
    {
        QFETCH(QByteArray, json);
        QVERIFY(FlathubClient::metadataFromSummary(json).isEmpty());
    }

    void hostileValuesStayInTheirKeys()
    {
        const QByteArray json = R"({"metadata": {"name": "org.example.App\n[Context]\nfilesystems=host", "permissions": {
            "shared": ["ipc\n[Context]\nfilesystems=host;"],
            "evil\nkey": ["host"],
            "session-bus": {"talk": ["org.example.A\nfilesystems=host", "org.freedesktop.Flatpak"]}
        }}})";
        const QByteArray metadata = FlathubClient::metadataFromSummary(json);
        // Only the well-formed bus name survives; nothing grants file access.
        QCOMPARE(kinds(metadata), QList<Kind>{Kind::RunAnyProgram});
    }

    void summaryUrl()
    {
        FlathubClient client(nullptr);
        PackageInfo package;
        package.id = u"org.gnome.Maps"_s;
        package.branch = u"stable"_s;
        QCOMPARE(client.summaryUrl(package), QUrl(u"https://flathub.org/api/v2/summary/org.gnome.Maps?branch=stable"_s));
        package.branch.clear();
        QCOMPARE(client.summaryUrl(package), QUrl(u"https://flathub.org/api/v2/summary/org.gnome.Maps"_s));
    }

    void fetchesFromServer()
    {
        FakeServer server;
        server.body = kSummary;
        FlathubClient client(nullptr, server.base());
        PackageInfo package;
        package.id = u"org.example.Notes"_s;
        package.branch = u"stable"_s;

        bool called = false;
        QByteArray metadata;
        client.fetchMetadata(package, [&](const QByteArray &result) {
            called = true;
            metadata = result;
        });
        QTRY_VERIFY(called);
        QCOMPARE(server.requestLine, QByteArray("GET /api/v2/summary/org.example.Notes?branch=stable HTTP/1.1"));
        QVERIFY(!Permissions::fromMetadata(metadata).isEmpty());
    }

    void notFoundMeansUnknown()
    {
        FakeServer server;
        server.status = "404 Not Found";
        server.body = R"({"detail": "App not found"})";
        FlathubClient client(nullptr, server.base());
        PackageInfo package;
        package.id = u"org.example.Missing"_s;

        bool called = false;
        QByteArray metadata = "unset";
        client.fetchMetadata(package, [&](const QByteArray &result) {
            called = true;
            metadata = result;
        });
        QTRY_VERIFY(called);
        QVERIFY(metadata.isEmpty());
    }

    void unreachableMeansUnknown()
    {
        quint16 port = 0;
        {
            QTcpServer closed;
            closed.listen(QHostAddress::LocalHost);
            port = closed.serverPort();
        }
        FlathubClient client(nullptr, QUrl(u"http://127.0.0.1:%1/api/v2/"_s.arg(port)));
        PackageInfo package;
        package.id = u"org.example.Notes"_s;

        bool called = false;
        QByteArray metadata = "unset";
        client.fetchMetadata(package, [&](const QByteArray &result) {
            called = true;
            metadata = result;
        });
        QTRY_VERIFY(called);
        QVERIFY(metadata.isEmpty());
    }
};

QTEST_GUILESS_MAIN(FlathubClientTest)
#include "flathubclienttest.moc"
