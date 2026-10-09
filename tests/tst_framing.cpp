// Ported from ljoss Tests/WireTests.swift: one read is not one message.
#include <QJsonDocument>
#include <QtEndian>
#include <QtTest>

#include "engine/Framing.h"

namespace framing = rigpa::framing;

class TestFraming : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip()
    {
        const QJsonObject object{{"id", "1"}, {"method", "engine.hello"}};
        const QByteArray frame = framing::encode(object);
        QCOMPARE(qFromLittleEndian<quint32>(frame.constData()), quint32(frame.size() - 4));
        framing::Reader reader;
        reader.append(frame);
        const auto objects = reader.take();
        QCOMPARE(objects.size(), 1);
        QCOMPARE(objects.first(), object);
    }

    void splitAcrossReads()
    {
        const QByteArray frame = framing::encode({{"ok", true}, {"id", "7"}});
        framing::Reader reader;
        for (char byte : frame) {
            QVERIFY(reader.take().isEmpty());
            reader.append(QByteArray(1, byte));
        }
        QCOMPARE(reader.take().size(), 1);
    }

    void coalescedInOneRead()
    {
        framing::Reader reader;
        reader.append(framing::encode({{"id", "1"}}) + framing::encode({{"id", "2"}})
                      + framing::encode({{"id", "3"}}).left(5));
        const auto objects = reader.take();
        QCOMPARE(objects.size(), 2);
        QCOMPARE(objects.at(1).value("id").toString(), QStringLiteral("2"));
        reader.append(framing::encode({{"id", "3"}}).mid(5));
        QCOMPARE(reader.take().size(), 1);
    }

    void oversizedLengthBreaksTheStream()
    {
        framing::Reader reader;
        QByteArray header(4, 0);
        qToLittleEndian<quint32>(quint32(framing::kMaxReplyBytes + 1), header.data());
        reader.append(header);
        QVERIFY(reader.broken());
        reader.append(framing::encode({{"id", "1"}}));
        QVERIFY(reader.take().isEmpty()); // it stays broken: the connection must go
    }

    void bodyMustBeAnObject()
    {
        framing::Reader reader;
        QByteArray body = "[1,2]";
        QByteArray header(4, 0);
        qToLittleEndian<quint32>(quint32(body.size()), header.data());
        reader.append(header + body);
        QVERIFY(reader.broken());
    }

    void requestCap()
    {
        QString error;
        const QJsonObject huge{{"blob", QString(framing::kMaxRequestBytes, QLatin1Char('x'))}};
        QVERIFY(framing::encode(huge, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestFraming)
#include "tst_framing.moc"
