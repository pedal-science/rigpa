// Structural edits on the editor's tree, and the tree written back as an engine document: a move
// within a chain, into a lane and out of one, a removal, and a round trip that keeps every node and
// every key the tree doesn't own.
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include "rig/RigEdits.h"

using namespace rigpa;

namespace {

QJsonObject fixture(const char *name)
{
    QFile f(QFINDTESTDATA(QStringLiteral("../dev/rigs/%1.json").arg(QLatin1String(name))));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

const QString kAmp = QStringLiteral("7A1C2E4F-0001-4000-8000-000000000001");
const QString kCab = QStringLiteral("7A1C2E4F-0001-4000-8000-000000000008");
const QString kSplit = QStringLiteral("6CC4CEC7-098A-4F38-970A-49793C98B2A5");
const QString kCutA = QStringLiteral("FB649601-CC86-458C-AACE-11DB8AFF779C");
const QString kCutB = QStringLiteral("BB0AE332-BD89-42BD-B406-708D577055AC");

QStringList order(const QList<RigElement> &series)
{
    QStringList out;
    for (const RigElement &e : series)
        out << e.element.left(8);
    return out;
}

// Apply → parse: what the engine would read back.
RigDocument reread(const RigDocument &doc)
{
    return RigDocument::parse(edits::toJson(doc));
}

} // namespace

class TestEdits : public QObject
{
    Q_OBJECT

private slots:
    void roundTripKeepsEverything()
    {
        for (const char *name : {"Clean", "Lead", "Dual"}) {
            const QJsonObject json = fixture(name);
            const RigDocument doc = RigDocument::parse(json);
            const QJsonObject out = edits::toJson(doc);
            QCOMPARE(out.value("name"), json.value("name"));
            const QJsonArray inChains = json.value("chains").toArray(), outChains = out.value("chains").toArray();
            QCOMPARE(outChains.size(), inChains.size());
            for (int c = 0; c < inChains.size(); ++c) {
                const QJsonObject a = inChains.at(c).toObject(), b = outChains.at(c).toObject();
                for (const QString &key : a.keys())
                    if (key != "nodes" && key != "connections")
                        QCOMPARE(b.value(key), a.value(key));
                // The same node objects, and the same edges (in any order).
                QCOMPARE(b.value("nodes").toArray().size(), a.value("nodes").toArray().size());
                for (const auto &n : a.value("nodes").toArray())
                    QVERIFY2(b.value("nodes").toArray().contains(n), name);
                QCOMPARE(b.value("connections").toArray().size(), a.value("connections").toArray().size());
                for (const auto &e : a.value("connections").toArray())
                    QVERIFY2(b.value("connections").toArray().contains(e), name);
            }
        }
    }

    void moveWithinAChain()
    {
        RigDocument doc = RigDocument::parse(fixture("Dual"));
        QCOMPARE(order(doc.chains[0].series), QStringList({"7A1C2E4F", "6CC4CEC7", "7A1C2E4F"}));
        // The cab to the front: slot 0, counted without the cab.
        QVERIFY(edits::move(doc, kCab, {0, {}, 0}));
        const RigDocument back = reread(doc);
        QCOMPARE(back.chains[0].series.first().element, kCab);
        QCOMPARE(back.chains[0].series.last().element, kSplit);
        QVERIFY(back.chains[0].editable);
    }

    void intoALaneAndOut()
    {
        RigDocument doc = RigDocument::parse(fixture("Dual"));
        // The amp into lane 1, after its cut.
        QVERIFY(edits::move(doc, kAmp, {0, {kSplit + "/1"}, 1}));
        RigDocument back = reread(doc);
        QVERIFY(back.chains[0].editable);
        QCOMPARE(back.chains[0].series.size(), 2);
        const RigElement &section = back.chains[0].series.first();
        QCOMPARE(section.kind, RigElement::Section);
        QCOMPARE(section.lanes[1].size(), 2);
        QCOMPARE(section.lanes[1][1].element, kAmp);
        QCOMPARE(edits::locate(back, kAmp).path, QStringList{kSplit + "/1"});

        // Lane 0's only block out, after the section: the lane stays, empty, straight through.
        QVERIFY(edits::move(back, kCutA, {0, {}, 1}));
        back = reread(back);
        QVERIFY(back.chains[0].editable);
        QCOMPARE(back.chains[0].series.size(), 3);
        QVERIFY(back.chains[0].series.first().lanes[0].isEmpty());
        QCOMPARE(back.chains[0].series[1].element, kCutA);
    }

    void refusals()
    {
        RigDocument doc = RigDocument::parse(fixture("Dual"));
        const QJsonObject before = edits::toJson(doc);
        QVERIFY(!edits::move(doc, kSplit, {0, {kSplit + "/0"}, 0}));  // into its own lanes
        QVERIFY(!edits::move(doc, kAmp, {0, {"nope/0"}, 0}));        // a lane that isn't there
        QVERIFY(!edits::move(doc, kAmp, {0, {}, 9}));                // past the end
        QVERIFY(!edits::move(doc, "nothing", {0, {}, 0}));
        QCOMPARE(edits::toJson(doc), before);                        // and nothing changed
    }

    void removeABlockAndASection()
    {
        RigDocument doc = RigDocument::parse(fixture("Dual"));
        QVERIFY(edits::remove(doc, kCutB));
        QVERIFY(edits::remove(doc, kSplit)); // the section, its lanes and what is in them
        const RigDocument back = reread(doc);
        QVERIFY(back.chains[0].editable);
        QCOMPARE(order(back.chains[0].series), QStringList({"7A1C2E4F", "7A1C2E4F"}));
        const QJsonArray nodes = edits::toJson(doc).value("chains").toArray()[0].toObject().value("nodes").toArray();
        QCOMPARE(nodes.size(), 4); // in, amp, cab, out
    }

    void laneCountAndGains()
    {
        RigDocument doc = RigDocument::parse(fixture("Dual"));
        QVERIFY(edits::setLaneCount(doc, kSplit, 3));
        QVERIFY(edits::setLaneGain(doc, kSplit, 0, {-6, false, false}));
        QVERIFY(edits::setLaneGain(doc, kSplit, 1, {-3, true, true}));   // muted: level and polarity kept
        QVERIFY(edits::setLaneGain(doc, kSplit, 2, {0, false, true}));   // inverted only
        RigDocument back = reread(doc);
        QVERIFY(back.chains[0].editable);
        const RigElement &s = back.chains[0].series[1];
        QCOMPARE(s.lanes.size(), 3);
        QCOMPARE(s.split.json.value("outputs").toInt(), 3);
        QCOMPARE(s.mix.json.value("inputs").toInt(), 3);
        // The gain nodes are the lanes' own, not tiles in them.
        QCOMPARE(s.lanes[0].size(), 1);
        QVERIFY(s.lanes[2].isEmpty());
        QVERIFY(qAbs(edits::laneGainOf(s, 0).levelDb + 6) < 1e-9);
        QVERIFY(qAbs(s.laneGains[0].json.value("gain").toDouble() - 0.501187) < 1e-5);
        QCOMPARE(edits::laneGainOf(s, 1).mute, true);
        QCOMPARE(edits::laneGainOf(s, 1).invert, true);
        QCOMPARE(edits::laneGainOf(s, 1).levelDb, -3.0);
        QCOMPARE(s.laneGains[2].json.value("gain").toDouble(), -1.0);
        QCOMPARE(s.laneGains[0].id, s.laneGains[0].element + "-gain"); // ljoss's form

        // Back to 0 dB, unmuted and upright: no gain node at all.
        QVERIFY(edits::setLaneGain(back, kSplit, 0, {}));
        QVERIFY(edits::setLaneCount(back, kSplit, 2));
        back = reread(back);
        QVERIFY(back.chains[0].series[1].laneGains[0].id.isEmpty());
        QCOMPARE(back.chains[0].series[1].lanes.size(), 2);
    }

    void newThings()
    {
        const RigElement s = edits::newSection(2);
        QCOMPARE(s.split.id, s.element + "-split");
        QCOMPARE(s.mix.json.value("inputs").toInt(), 2);
        RigDocument doc = RigDocument::parse(fixture("Clean"));
        QVERIFY(edits::insert(doc, s, {0, {}, 1}));
        const RigDocument back = reread(doc);
        QVERIFY(back.chains[0].editable);
        QCOMPARE(back.chains[0].series[1].kind, RigElement::Section);
        QCOMPARE(back.chains[0].series[1].lanes.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestEdits)
#include "tst_edits.moc"
