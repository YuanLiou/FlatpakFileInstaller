// SPDX-License-Identifier: Apache-2.0
#include "fakeserver.h"
#include "icondownloader.h"

#include <QBuffer>
#include <QTest>

using namespace Qt::StringLiterals;

namespace
{

QByteArray png(int size)
{
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return data;
}

QImage fetch(const QUrl &url)
{
    IconDownloader downloader(nullptr);
    bool called = false;
    QImage icon(1, 1, QImage::Format_ARGB32);
    downloader.fetch(url, [&](const QImage &result) {
        called = true;
        icon = result;
    });
    if (!QTest::qWaitFor([&] {
            return called;
        })) {
        qWarning("no answer");
    }
    return icon;
}

} // namespace

class IconDownloaderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void isAllowed_data()
    {
        QTest::addColumn<QString>("icon");
        QTest::addColumn<bool>("allowed");
        QTest::newRow("same host") << u"https://dl.flathub.org/repo/appstream/x86_64/icons/128x128/org.example.Notes.png"_s << true;
        QTest::newRow("host case") << u"https://DL.flathub.org/icon.png"_s << true;
        QTest::newRow("other host") << u"https://tracker.example.com/icon.png"_s << false;
        QTest::newRow("plain http") << u"http://dl.flathub.org/icon.png"_s << false;
        QTest::newRow("local file") << u"file:///etc/passwd"_s << false;
        QTest::newRow("empty") << QString() << false;
    }

    void isAllowed()
    {
        QFETCH(QString, icon);
        QFETCH(bool, allowed);
        QCOMPARE(IconDownloader::isAllowed(QUrl(icon), QUrl(u"https://dl.flathub.org/repo/"_s)), allowed);
    }

    void downloadsIcon()
    {
        FakeServer server;
        server.contentType = "image/png";
        server.body = png(64);
        const QImage icon = fetch(server.base().resolved(QUrl(u"icon.png"_s)));
        QCOMPARE(icon.size(), QSize(64, 64));
        QCOMPARE(server.requestLine, QByteArray("GET /api/v2/icon.png HTTP/1.1"));
    }

    void scalesDownBigIcons()
    {
        FakeServer server;
        server.contentType = "image/png";
        server.body = png(512);
        QCOMPARE(fetch(server.base()).size(), QSize(256, 256));
    }

    void notFoundGivesNoIcon()
    {
        FakeServer server;
        server.status = "404 Not Found";
        server.body = "missing";
        QVERIFY(fetch(server.base()).isNull());
    }

    void notAnImageGivesNoIcon()
    {
        FakeServer server;
        server.contentType = "text/html";
        server.body = "<html></html>";
        QVERIFY(fetch(server.base()).isNull());
    }
};

QTEST_GUILESS_MAIN(IconDownloaderTest)
#include "icondownloadertest.moc"
