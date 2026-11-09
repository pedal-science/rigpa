// pedald's event stream, as bytes: what PedaldEvents::parse makes of what the service sends.
#include <QtTest>

#include "pedald/PedaldEvents.h"

using namespace rigpa;

class TestPedaldEvents : public QObject
{
    Q_OBJECT

private slots:
    void oneEvent()
    {
        QByteArray buffer("id: run-3\nevent: running\ndata: {\"rig\": \"Clean\", \"position\": 0}\n\n");
        const auto events = PedaldEvents::parse(buffer);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events[0].id, QStringLiteral("run-3"));
        QCOMPARE(events[0].kind, QStringLiteral("running"));
        QCOMPARE(events[0].data.value("rig").toString(), QStringLiteral("Clean"));
        QVERIFY(buffer.isEmpty());
    }

    void halfAnEventWaits()
    {
        QByteArray buffer("id: a-1\nevent: saved\ndata: {\"rig\": \"X\"}\n\nid: a-2\nevent: hist");
        QCOMPARE(PedaldEvents::parse(buffer).size(), 1);
        QCOMPARE(buffer, QByteArray("id: a-2\nevent: hist"));
        buffer += "ory\ndata: {\"undo\": 2}\n\n";
        const auto events = PedaldEvents::parse(buffer);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events[0].kind, QStringLiteral("history"));
        QCOMPARE(events[0].data.value("undo").toInt(), 2);
    }

    void keepAlivesAndCrLfAreNothing()
    {
        QByteArray buffer(": ka\n\n: ka\r\n\r\nevent: bank\r\ndata: {}\r\n\r\n");
        const auto events = PedaldEvents::parse(buffer);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events[0].kind, QStringLiteral("bank"));
    }

    void dataOnSeveralLinesIsJoined()
    {
        QByteArray buffer("event: x\ndata: {\"a\":\ndata: 1}\n\n");
        QCOMPARE(PedaldEvents::parse(buffer).at(0).data.value("a").toInt(), 1);
    }
};

QTEST_MAIN(TestPedaldEvents)
#include "tst_pedaldevents.moc"
