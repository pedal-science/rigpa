// A frame the engine pushes, with no id, is an event: EngineClient passes it on and the request it
// arrived in the middle of is still answered. The hello names the client.
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include "engine/EngineClient.h"
#include "engine/Framing.h"

using namespace rigpa;

class TestEngineEvents : public QObject
{
    Q_OBJECT
    QTcpServer server;
    QTcpSocket *socket = nullptr;
    framing::Reader reader;
    QList<QJsonObject> requests;

    void send(const QJsonObject &object) { socket->write(framing::encode(object)); }

private slots:
    void initTestCase()
    {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [this] {
            socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this] {
                reader.append(socket->readAll());
                for (const QJsonObject &request : reader.take()) {
                    requests.append(request);
                    if (request.value("method").toString() == QLatin1String("engine.hello"))
                        send({{"id", request.value("id")}, {"ok", true}, {"features", QJsonArray{"events"}}});
                }
            });
        });
    }

    void anEventIsPassedOnAndARequestStillAnswered()
    {
        EngineClient client;
        client.setClientName("rigpa");
        QSignalSpy events(&client, &EngineClient::event);
        client.connectTo("127.0.0.1", server.serverPort(), "token");
        QTRY_VERIFY(client.isReady());
        QCOMPARE(requests.first().value("client").toObject().value("name").toString(), QStringLiteral("rigpa"));

        bool answered = false;
        client.request("engine.status", {}, [&](const EngineReply &reply) { answered = reply.ok; });
        QTRY_COMPARE(requests.size(), 2);
        // The engine pushes an event before it answers.
        send({{"event", "rig.params"}, {"seq", 7}, {"values", QJsonArray{}}});
        send({{"id", requests.last().value("id")}, {"ok", true}});
        QTRY_VERIFY(answered);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().first().toJsonObject().value("seq").toInt(), 7);
    }
};

QTEST_MAIN(TestEngineEvents)
#include "tst_engineevents.moc"
