// EngineClient against a fake namchaind: a QTcpServer speaking the framing, with a script for
// how it answers.
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include "engine/EngineClient.h"
#include "engine/Framing.h"

using rigpa::EngineClient;
using rigpa::EngineReply;
namespace framing = rigpa::framing;

namespace {

class FakeEngine : public QObject
{
public:
    QString token = QStringLiteral("0123456789abcdef0123");
    bool answer = true;       // false: swallow requests (a stuck daemon)
    int connections = 0;
    QStringList methods;

    FakeEngine()
    {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = server.nextPendingConnection()) {
                ++connections;
                auto *reader = new framing::Reader;
                connect(socket, &QTcpSocket::disconnected, socket, [socket, reader] {
                    delete reader;
                    socket->deleteLater();
                });
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket, reader] {
                    reader->append(socket->readAll());
                    for (const QJsonObject &request : reader->take())
                        reply(socket, request);
                });
            }
        });
    }
    quint16 port() const { return server.serverPort(); }
    void dropAll()
    {
        for (QTcpSocket *s : server.findChildren<QTcpSocket *>())
            s->abort();
    }

private:
    void reply(QTcpSocket *socket, const QJsonObject &request)
    {
        const QString method = request.value("method").toString();
        methods.append(method);
        QJsonObject out{{"id", request.value("id")}};
        if (method == QLatin1String("engine.hello")) {
            if (request.value("token").toString() != token) {
                out.insert("ok", false);
                out.insert("error", QJsonObject{{"code", "unauthorized"}, {"message", "bad token"}});
            } else {
                out.insert("ok", true);
                out.insert("protocol", 2);
                out.insert("version", "0.1.0");
                out.insert("features", QJsonArray{"levels", "schema"});
            }
        } else if (!answer) {
            return;
        } else if (method == QLatin1String("engine.status")) {
            out.insert("ok", true);
            out.insert("status", QJsonObject{{"state", "running"}});
        } else {
            out.insert("ok", false);
            out.insert("error", QJsonObject{{"code", "unknown_method"}, {"message", method}});
        }
        socket->write(framing::encode(out));
    }

    QTcpServer server;
};

} // namespace

class TestEngineClient : public QObject
{
    Q_OBJECT

private slots:
    void helloThenRequests()
    {
        FakeEngine engine;
        EngineClient client;
        QSignalSpy ready(&client, &EngineClient::ready);
        client.connectTo("127.0.0.1", engine.port(), engine.token);
        QVERIFY(ready.wait(2000));
        QVERIFY(client.hasFeature("levels"));
        QCOMPARE(client.version(), QStringLiteral("0.1.0"));

        EngineReply got;
        bool done = false;
        client.request("engine.status", {}, [&](const EngineReply &r) { got = r; done = true; });
        QTRY_VERIFY(done);
        QVERIFY(got.ok);
        QCOMPARE(got.body.value("status").toObject().value("state").toString(), QStringLiteral("running"));

        done = false;
        client.request("no.such", {}, [&](const EngineReply &r) { got = r; done = true; });
        QTRY_VERIFY(done);
        QVERIFY(!got.ok);
        QCOMPARE(got.code, QStringLiteral("unknown_method"));
    }

    void wrongTokenIsNotRetried()
    {
        FakeEngine engine;
        EngineClient client;
        client.setBackoff({20});
        client.connectTo("127.0.0.1", engine.port(), "wrong-token-wrong-token");
        QTRY_COMPARE(client.state(), EngineClient::State::Refused);
        QVERIFY(client.lastError().contains("unauthorized"));
        QTest::qWait(150);
        QCOMPARE(engine.connections, 1);
    }

    void reconnectsAfterADrop()
    {
        FakeEngine engine;
        EngineClient client;
        client.setBackoff({30, 30});
        QSignalSpy ready(&client, &EngineClient::ready);
        client.connectTo("127.0.0.1", engine.port(), engine.token);
        QVERIFY(ready.wait(2000));
        engine.dropAll();
        QTRY_COMPARE(ready.count(), 2);
        QCOMPARE(engine.connections, 2);
    }

    void retriesWhileNothingListens()
    {
        quint16 port;
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            port = probe.serverPort();
        } // closed: connecting is refused
        EngineClient client;
        client.setBackoff({40});
        client.connectTo("127.0.0.1", port, "0123456789abcdef0123");
        QTRY_COMPARE(client.state(), EngineClient::State::Waiting);
        QVERIFY(!client.lastError().isEmpty());
    }

    void notConnectedFailsAtOnce()
    {
        EngineClient client;
        EngineReply got;
        client.request("engine.status", {}, [&](const EngineReply &r) { got = r; });
        QCOMPARE(got.code, QStringLiteral("not_connected"));
    }

    void aStuckEngineTimesOut()
    {
        FakeEngine engine;
        engine.answer = false;
        EngineClient client;
        client.setReplyTimeout(200);
        client.setBackoff({5000});
        QSignalSpy ready(&client, &EngineClient::ready);
        client.connectTo("127.0.0.1", engine.port(), engine.token);
        QVERIFY(ready.wait(2000));
        EngineReply got;
        bool done = false;
        client.request("engine.status", {}, [&](const EngineReply &r) { got = r; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 3000);
        QCOMPARE(got.code, QStringLiteral("disconnected"));
        QCOMPARE(client.state(), EngineClient::State::Waiting);
    }
};

QTEST_GUILESS_MAIN(TestEngineClient)
#include "tst_engineclient.moc"
