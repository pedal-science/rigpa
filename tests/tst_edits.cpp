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

    // --- The tuner's ephemeral node (namchain docs/tuner.md) -------------------

    void injectPutsTheTunerFirstAfterTheInput()
    {
        QJsonObject doc = fixture("Lead");
        const QJsonObject before = doc.value("chains").toArray().at(0).toObject();
        QCOMPARE(edits::injectTuner(doc), 1);
        QVERIFY(edits::hasTuner(doc));

        const QJsonObject chain = doc.value("chains").toArray().at(0).toObject();
        const QJsonArray nodes = chain.value("nodes").toArray();
        QCOMPARE(nodes.size(), before.value("nodes").toArray().size() + 1);
        // The input, then the tuner, exactly as "the first node after the input" asks.
        QCOMPARE(nodes.at(0).toObject().value("type").toString(), QStringLiteral("nc.input"));
        QCOMPARE(nodes.at(1).toObject().value("type").toString(), QStringLiteral("nc.tuner"));
        QCOMPARE(nodes.at(1).toObject().value("meta").toObject().value("ephemeral").toString(),
                 QStringLiteral("tuner"));

        // Everything the input fed now hangs off the tuner, and the input feeds only the tuner: what
        // makes the whole chain silent.
        const QJsonArray connections = chain.value("connections").toArray();
        int outOfInput = 0, outOfTuner = 0;
        for (const QJsonValue &v : connections) {
            const QJsonObject c = v.toObject();
            if (c.value("from").toString() == QStringLiteral("in")) {
                ++outOfInput;
                QCOMPARE(c.value("to").toString(), QStringLiteral("tuner"));
            }
            if (c.value("from").toString() == QStringLiteral("tuner"))
                ++outOfTuner;
        }
        QCOMPARE(outOfInput, 1);
        // One for each node the input fed in the original document.
        int fed = 0;
        for (const QJsonValue &v : before.value("connections").toArray())
            if (v.toObject().value("from").toString() == QStringLiteral("in"))
                ++fed;
        QCOMPARE(outOfTuner, fed);
    }

    void injectCarriesTheSwitch()
    {
        // A muted tuner is the node's default, so the document says nothing: the short form is the
        // common one, and a document a person reads is not cluttered with it.
        QJsonObject muted = fixture("Clean");
        QCOMPARE(edits::injectTuner(muted, true), 1);
        const QJsonObject mutedNode = muted.value("chains").toArray().at(0).toObject()
                                        .value("nodes").toArray().at(1).toObject();
        QVERIFY(!mutedNode.contains("mute"));

        // Open, it has to say so, or the rig would be muted by the node it was told not to mute.
        QJsonObject open = fixture("Clean");
        QCOMPARE(edits::injectTuner(open, false), 1);
        const QJsonObject openNode = open.value("chains").toArray().at(0).toObject()
                                       .value("nodes").toArray().at(1).toObject();
        QCOMPARE(openNode.value("mute").toInt(-1), 0);

        // And the switch is not part of the node's identity: stripping an open one still strips it.
        QCOMPARE(edits::stripTuner(open), 1);
        QVERIFY(!edits::hasTuner(open));
    }

    void injectReachesEveryChain()
    {
        QJsonObject doc = fixture("Dual");
        QCOMPARE(doc.value("chains").toArray().size(), 2);
        QCOMPARE(edits::injectTuner(doc), 2);
        for (const QJsonValue &v : doc.value("chains").toArray()) {
            const QJsonArray nodes = v.toObject().value("nodes").toArray();
            int tuners = 0;
            for (const QJsonValue &n : nodes)
                if (n.toObject().value("type").toString() == QStringLiteral("nc.tuner"))
                    ++tuners;
            QCOMPARE(tuners, 1);
            QCOMPARE(nodes.at(1).toObject().value("type").toString(), QStringLiteral("nc.tuner"));
        }
    }

    void stripIsTheInverseOfInject()
    {
        for (const char *name : {"Clean", "Lead", "Dual"}) {
            QJsonObject doc = fixture(name);
            const QJsonObject original = doc;
            QCOMPARE(edits::injectTuner(doc), doc.value("chains").toArray().size());
            QCOMPARE(edits::stripTuner(doc), doc.value("chains").toArray().size());
            QVERIFY(!edits::hasTuner(doc));
            // Every node and every wire, byte for byte: the epilogue of a tuner session is the
            // document it started with.
            QCOMPARE(doc.value("chains").toArray(), original.value("chains").toArray());
        }
    }

    void injectIsIdempotent()
    {
        QJsonObject doc = fixture("Clean");
        QCOMPARE(edits::injectTuner(doc), 1);
        QCOMPARE(edits::injectTuner(doc), 0);
        int tuners = 0;
        for (const QJsonValue &n : doc.value("chains").toArray().at(0).toObject().value("nodes").toArray())
            if (n.toObject().value("type").toString() == QStringLiteral("nc.tuner"))
                ++tuners;
        QCOMPARE(tuners, 1);
    }

    void injectAvoidsAnIdTheChainUses()
    {
        QJsonObject doc = fixture("Clean");
        QJsonArray chains = doc.value("chains").toArray();
        QJsonObject chain = chains.at(0).toObject();
        QJsonArray nodes = chain.value("nodes").toArray();
        nodes.append(QJsonObject{{"id", "tuner"}, {"type", "nc.gain"}});
        chain.insert("nodes", nodes);
        chains.replace(0, chain);
        doc.insert("chains", chains);

        QCOMPARE(edits::injectTuner(doc), 1);
        QString injected;
        for (const QJsonValue &n : doc.value("chains").toArray().at(0).toObject().value("nodes").toArray())
            if (n.toObject().value("type").toString() == QStringLiteral("nc.tuner"))
                injected = n.toObject().value("id").toString();
        QCOMPARE(injected, QStringLiteral("tuner-1"));
    }

    void abareGraphGetsOneToo()
    {
        // A one-chain document may be written with its graph at the top (rigpa's toJson does), and
        // the splice has to work there or a bare rig would be the one rig a tuner cannot mute.
        QJsonObject doc = fixture("Clean");
        const QJsonArray chain = doc.value("chains").toArray();
        const QJsonObject first = chain.at(0).toObject();
        QJsonObject bare;
        bare.insert("name", doc.value("name"));
        bare.insert("nodes", first.value("nodes"));
        bare.insert("connections", first.value("connections"));

        QCOMPARE(edits::injectTuner(bare), 1);
        QVERIFY(edits::hasTuner(bare));
        QCOMPARE(bare.value("nodes").toArray().at(1).toObject().value("type").toString(),
                 QStringLiteral("nc.tuner"));
        QCOMPARE(edits::stripTuner(bare), 1);
        QVERIFY(!edits::hasTuner(bare));
        QCOMPARE(bare.value("nodes").toArray(), first.value("nodes").toArray());
        QCOMPARE(bare.value("connections").toArray(), first.value("connections").toArray());
    }

    void stripLeavesAHandWrittenTunerAlone()
    {
        // A rig may hold a tuner node of its own; only the one a client mounted carries the marker,
        // and only that one is the client's to take away.
        QJsonObject doc = fixture("Clean");
        QJsonArray chains = doc.value("chains").toArray();
        QJsonObject chain = chains.at(0).toObject();
        QJsonArray nodes = chain.value("nodes").toArray();
        nodes.append(QJsonObject{{"id", "mine"}, {"type", "nc.tuner"}});
        chain.insert("nodes", nodes);
        chains.replace(0, chain);
        doc.insert("chains", chains);

        QVERIFY(!edits::hasTuner(doc));
        QCOMPARE(edits::stripTuner(doc), 0);
        int tuners = 0;
        for (const QJsonValue &n : doc.value("chains").toArray().at(0).toObject().value("nodes").toArray())
            if (n.toObject().value("type").toString() == QStringLiteral("nc.tuner"))
                ++tuners;
        QCOMPARE(tuners, 1);
    }

    void anEmptyChainMutesToo()
    {
        // Input straight to output: the tuner is still the first node after the input.
        QJsonObject doc;
        QJsonObject chain;
        chain.insert("id", QStringLiteral("only"));
        chain.insert("nodes", QJsonArray{QJsonObject{{"id", "in"}, {"type", "nc.input"}},
                                         QJsonObject{{"id", "out"}, {"type", "nc.output"}}});
        chain.insert("connections", QJsonArray{QJsonObject{{"from", "in"}, {"to", "out"}}});
        doc.insert("chains", QJsonArray{chain});

        QCOMPARE(edits::injectTuner(doc), 1);
        const QJsonObject after = doc.value("chains").toArray().at(0).toObject();
        QCOMPARE(after.value("nodes").toArray().size(), 3);
        QCOMPARE(edits::stripTuner(doc), 1);
        QCOMPARE(doc.value("chains").toArray().at(0).toObject(), chain);
    }
};

QTEST_GUILESS_MAIN(TestEdits)
#include "tst_edits.moc"
