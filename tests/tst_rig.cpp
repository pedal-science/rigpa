#include <QFile>
#include <QJsonDocument>
#include <QtTest>

#include "rig/RigDocument.h"
#include "rig/RigEditorModel.h"
#include "rig/RigSession.h"

using namespace rigpa;

namespace {
QJsonObject fixture(const char *name)
{
    QFile f(QFINDTESTDATA(QStringLiteral("../dev/rigs/%1.json").arg(QLatin1String(name))));
    if (!f.open(QIODevice::ReadOnly))
        qFatal("no fixture %s", name);
    return QJsonDocument::fromJson(f.readAll()).object();
}
QJsonObject json(const char *text) { return QJsonDocument::fromJson(text).object(); }
} // namespace

class TestRig : public QObject
{
    Q_OBJECT

private slots:
    void blocksGroupByElement()
    {
        const RigDocument doc = RigDocument::parse(fixture("Clean"));
        QCOMPARE(doc.name, QStringLiteral("Clean"));
        QCOMPARE(doc.chains.size(), 1);
        const RigChain &c = doc.chains.first();
        QVERIFY2(c.editable, qPrintable(c.problem));
        QCOMPARE(c.series.size(), 2);            // amp (nam + tone stack) and cab
        QCOMPARE(c.series[0].nodes.size(), 2);
        QCOMPARE(c.series[0].nodes[1].type, QStringLiteral("fx.amp.tonestack"));
        QCOMPARE(c.series[1].nodes.first().type, QStringLiteral("fx.cab.ir"));
    }

    void sectionsAndLanes()
    {
        const RigDocument doc = RigDocument::parse(fixture("Dual"));
        QCOMPARE(doc.chains.size(), 2);
        const RigChain &g = doc.chains.first();
        QVERIFY2(g.editable, qPrintable(g.problem));
        QCOMPARE(g.series.size(), 3);            // amp, section, cab
        const RigElement &s = g.series[1];
        QCOMPARE(s.kind, RigElement::Section);
        QCOMPARE(s.lanes.size(), 2);
        QCOMPARE(s.lanes[0].first().nodes.first().json.value("high_cut").toInt(), 8000); // port 0 first
        QCOMPARE(s.lanes[1].first().nodes.first().json.value("low_cut").toInt(), 80);
        QList<const RigElement *> flat;
        RigDocument::flatten(g.series, flat);
        QCOMPARE(flat.size(), 5);                // amp, section, lane 0, lane 1, cab
        QCOMPARE(flat[1]->kind, RigElement::Section);
    }

    void emptyLaneAndBareGraph()
    {
        const RigDocument doc = RigDocument::parse(json(R"({"nodes":[
            {"id":"in","type":"nc.input"},{"id":"s","type":"nc.split","outputs":2,"element":"S"},
            {"id":"g","type":"nc.gain"},{"id":"m","type":"nc.mix","inputs":2,"element":"S"},
            {"id":"out","type":"nc.output"}],
            "connections":[{"from":"in","to":"s"},{"from":"s","from_port":0,"to":"g"},
            {"from":"g","to":"m","to_port":0},{"from":"s","from_port":1,"to":"m","to_port":1},
            {"from":"m","to":"out"}]})"));
        QCOMPARE(doc.chains.size(), 1);
        const RigChain &c = doc.chains.first();
        QVERIFY2(c.editable, qPrintable(c.problem));
        QCOMPARE(c.series.first().lanes.size(), 2);
        QVERIFY(c.series.first().lanes[1].isEmpty()); // a dry lane
    }

    void aFanOutIsShownNotEdited()
    {
        const RigDocument doc = RigDocument::parse(json(R"({"chains":[{"id":"c","nodes":[
            {"id":"in","type":"nc.input"},{"id":"a","type":"nc.gain"},{"id":"b","type":"nc.gain"},
            {"id":"out","type":"nc.output"}],
            "connections":[{"from":"in","to":"a"},{"from":"a","to":"b"},{"from":"a","to":"out"}]}]})"));
        const RigChain &c = doc.chains.first();
        QVERIFY(!c.editable);
        QVERIFY(!c.problem.isEmpty());
        QCOMPARE(c.series.size(), 2); // every node, as written
    }

    void layoutOrderAndWrapping()
    {
        RigEditorModel model(nullptr, nullptr);
        model.setAvailableWidth(1200);
        model.layout(RigDocument::parse(fixture("Dual")), {});
        QCOMPARE(model.selectableCount(), 6); // guitar: amp, section, 2 lanes, cab; vocal: gain
        // The selection order is the signal order, a section before its lanes.
        const auto &items = model.items();
        QStringList kinds;
        for (int s = 0; s < model.selectableCount(); ++s)
            for (const auto &i : items)
                if (i.select == s)
                    kinds << i.kind;
        QCOMPARE(kinds, (QStringList{"block", "section", "block", "block", "block", "block"}));
        // Lanes sit inside their section's frame.
        const QRectF frame = model.rectOf(1);
        QVERIFY(frame.contains(model.rectOf(2)) && frame.contains(model.rectOf(3)));
        QVERIFY(model.rectOf(2).bottom() <= model.rectOf(3).top()); // stacked
        const qreal wide = model.contentHeight();

        model.setAvailableWidth(520);  // narrow: the guitar chain wraps onto more rows
        model.layout(RigDocument::parse(fixture("Dual")), {});
        QVERIFY(model.contentHeight() > wide);
        QVERIFY(model.rectOf(4).top() > model.rectOf(0).bottom()); // the cab moved down a row
        QCOMPARE(model.selectOf(model.elementAt(4)), 4);
    }

    void missingAndBypassed()
    {
        QJsonObject doc = fixture("Clean");
        RigEditorModel model(nullptr, nullptr);
        const RigDocument parsed = RigDocument::parse(doc);
        const QString cab = parsed.chains[0].series[1].nodes[0].id;
        model.layout(parsed, {cab});
        bool sawMissing = false;
        for (const auto &i : model.items())
            if (i.kind == QLatin1String("block") && i.missing)
                sawMissing = true;
        QVERIFY(sawMissing);
    }

    void runningRigByFileThenByName()
    {
        const QStringList names{"Clean", "Lead"};
        QCOMPARE(RigSession::runningName("/lib/rigs/Lead.json", "Lead", names), QStringLiteral("Lead"));
        // After a boot the engine runs last.json; the document's name says which rig it is.
        QCOMPARE(RigSession::runningName("/lib/rigs/last.json", "Clean", names), QStringLiteral("Clean"));
        QCOMPARE(RigSession::runningName("", "From ljoss", names), QString());
    }
};

QTEST_GUILESS_MAIN(TestRig)
#include "tst_rig.moc"
