// The two layouts (LayoutSpec) and what the compact one adds: the play view's chips (PlayModel) and
// where the footswitches are (PedaldStatus). The bar's numbers are pinned here, so a change made
// for the 4.3" panel can't move the bar.
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "app/LayoutSpec.h"
#include "pedald/PedaldStatus.h"
#include "rig/FlowEqModel.h"
#include "rig/PlayModel.h"
#include "rig/RigDocument.h"
#include "rig/RigEditorModel.h"
#include "rig/TunerModel.h"

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

class TestLayout : public QObject
{
    Q_OBJECT

private slots:
    void wideIsTheBarAsItWas()
    {
        const LayoutSpec wide(LayoutSpec::Kind::Wide);
        QCOMPARE(wide.name(), QStringLiteral("wide"));
        QVERIFY(!wide.compact());
        QCOMPARE(wide.screenWidth(), 1920);
        QCOMPARE(wide.screenHeight(), 480);
        QCOMPARE(wide.menuWidth(), 96);
        QCOMPARE(wide.headerHeight(), 64);
        QCOMPARE(wide.footerHeight(), 48);
        QCOMPARE(wide.infoWidth(), 320);
        QCOMPARE(wide.contentWidth(), 1504);
        QCOMPARE(wide.contentHeight(), 368);
        QCOMPARE(wide.tunerBarWidth(), TunerModel::kBarWidth);
        QCOMPARE(wide.tunerNoteWidth(), 280);
        QCOMPARE(wide.tunerScaleHeight(), 200);
        QCOMPARE(wide.flowEqCurveWidth(), FlowEqModel::kCurveWidth);
        QCOMPARE(wide.flowEqCurveHeight(), FlowEqModel::kCurveHeight);
        QCOMPARE(wide.keyboardWidth(), 880);
        // The editor's tiles are the struct's own defaults, which the bar has always used.
        const RigEditorModel::Style style = wide.editorStyle(), bar;
        QCOMPARE(style.tileWidth, bar.tileWidth);
        QCOMPARE(style.tileHeight, bar.tileHeight);
        QCOMPARE(style.rowSpacing, bar.rowSpacing);
        QCOMPARE(style.padding, bar.padding);
    }

    void compactIsThePanel()
    {
        const LayoutSpec compact(LayoutSpec::Kind::Compact);
        QCOMPARE(compact.name(), QStringLiteral("compact"));
        QVERIFY(compact.compact());
        QCOMPARE(compact.screenWidth(), 800);
        QCOMPARE(compact.screenHeight(), 480);
        QCOMPARE(compact.menuWidth(), 0);
        QCOMPARE(compact.infoWidth(), 0);
        QCOMPARE(compact.contentWidth(), 800);
        QCOMPARE(compact.contentHeight(), 480 - 52 - 40);
        QCOMPARE(compact.tunerBarWidth(), 560);
        QCOMPARE(compact.keyboardWidth(), 800);
        // flowEQ's curve narrows so its eight controls fit beside it, four to a row.
        QCOMPARE(compact.flowEqCurveWidth(), 280);
        QCOMPARE(compact.flowEqCurveHeight(), 188);
        QCOMPARE(compact.editorStyle().tileWidth, 132.0);
        QCOMPARE(compact.editorStyle().tileHeight, 72.0);
        // The bar's type: the panel is as dense, so the same size reads the same.
        QVERIFY(compact.playTitleFont() >= 60);
    }

    void aLayoutIsNamed()
    {
        QCOMPARE(LayoutSpec::kindNamed(QStringLiteral("compact")), LayoutSpec::Kind::Compact);
        QCOMPARE(LayoutSpec::kindNamed(QStringLiteral("wide")), LayoutSpec::Kind::Wide);
    }

    // The chain editor on the panel: 6 px inside the 800 px content on either side.
    void theCompactEditorPutsAChainAcrossThePanel()
    {
        RigEditorModel model(nullptr, nullptr);
        model.setStyle(LayoutSpec(LayoutSpec::Kind::Compact).editorStyle());
        model.setAvailableWidth(788);
        model.layout(RigDocument::parse(fixture("Dual")), {});
        QCOMPARE(model.rectOf(0).size(), QSizeF(132, 72));
        // Dual's guitar chain (amp, a two-lane section, cab) fits on one row.
        QCOMPARE(model.rectOf(4).center().y(), model.rectOf(0).center().y());
        QVERIFY(model.contentWidth() <= 788);
        // And the bar's style at the same width wraps it.
        RigEditorModel bar(nullptr, nullptr);
        bar.setAvailableWidth(788);
        bar.layout(RigDocument::parse(fixture("Dual")), {});
        QVERIFY(bar.rectOf(4).top() > bar.rectOf(0).bottom());
    }

    // The play view's chips: every block in signal order, a section's lanes included and the
    // section itself not, with the tile's name.
    void playChipsAreTheBlocksInSignalOrder()
    {
        RigEditorModel editor(nullptr, nullptr);
        PlayModel play(nullptr, &editor, nullptr);
        editor.layout(RigDocument::parse(fixture("Dual")), {});
        QCOMPARE(play.count(), 5); // guitar: amp, two lanes, cab; vocal: gain
        const QList<int> blocks{0, 2, 3, 4, 5}; // the editor's selectables, less the section (1)
        for (int i = 0; i < blocks.size(); ++i) {
            QCOMPARE(play.elementAt(i), editor.elementAt(blocks.at(i)));
            const RigEditorModel::Item *tile = editor.itemFor(play.elementAt(i));
            QVERIFY(tile);
            QCOMPARE(play.data(play.index(i), PlayModel::TitleRole).toString(), tile->title);
        }
        QVERIFY(play.elementAt(5).isEmpty());
        play.toggle(0); // no session: nothing to switch, and no crash
    }

    void aSwitchedBlockKeepsItsChip()
    {
        RigEditorModel editor(nullptr, nullptr);
        PlayModel play(nullptr, &editor, nullptr);
        const QJsonObject on = fixture("Clean");
        editor.layout(RigDocument::parse(on), {});
        QCOMPARE(play.count(), 2);
        QVERIFY(!play.data(play.index(1), PlayModel::BypassedRole).toBool());

        // The cab switched off elsewhere: the same chips, one of them changed in place.
        QJsonObject off = on;
        QJsonArray nodes = off.value("chains").toArray()[0].toObject().value("nodes").toArray();
        const QString cab = play.elementAt(1);
        for (int i = 0; i < nodes.size(); ++i) {
            QJsonObject n = nodes[i].toObject();
            if (n.value("element").toString() == cab) {
                n.insert("bypass", true);
                nodes[i] = n;
            }
        }
        QJsonArray chains = off.value("chains").toArray();
        QJsonObject chain = chains[0].toObject();
        chain.insert("nodes", nodes);
        chains[0] = chain;
        off.insert("chains", chains);

        QSignalSpy reset(&play, &QAbstractItemModel::modelReset);
        QSignalSpy changedData(&play, &QAbstractItemModel::dataChanged);
        editor.layout(RigDocument::parse(off), {});
        QCOMPARE(reset.count(), 0);
        QCOMPARE(changedData.count(), 1);
        QVERIFY(play.data(play.index(1), PlayModel::BypassedRole).toBool());
    }

    void whereTheFootswitchesAre()
    {
        PedaldClient client;
        PedaldStatus status(&client);
        QSignalSpy changed(&status, &PedaldStatus::changed);
        QVERIFY(!status.reachable());

        const QJsonObject warm = json(R"({"bank": {"banks": [["A", "B", "C"], ["D"]], "bank": 0,
            "position": 1, "size": 3, "current": "B"}, "engine": {"warm_decision": {"wanted": "A"}}})");
        status.applyStatus(warm);
        QVERIFY(status.reachable());
        QCOMPARE(status.bank(), 0);
        QCOMPARE(status.bankCount(), 2);
        QCOMPARE(status.position(), 1);
        QCOMPARE(status.size(), 3);
        QCOMPARE(status.next(), QStringLiteral("A")); // the way the player last stepped: the service's word
        QCOMPARE(changed.count(), 1);
        status.applyStatus(warm);
        QCOMPARE(changed.count(), 1); // the same status twice is one change

        // Warming off: the service's `wanted` is stale then, and the next rig up stands in,
        // wrapping at the end of the bank.
        status.applyStatus(json(R"({"bank": {"banks": [["A", "B", "C"]], "bank": 0, "position": 2,
            "size": 3}, "engine": {"warm_decision": {"state": "off", "wanted": "B"}}})"));
        QCOMPARE(status.next(), QStringLiteral("A"));
        status.applyStatus(json(R"({"bank": {"banks": [["A", "B", "C"]], "bank": 0, "position": 0,
            "size": 3}, "engine": {"warm_decision": {"state": "idle", "wanted": null}}})"));
        QCOMPARE(status.next(), QStringLiteral("B"));

        // A rig the bank doesn't have: no place in it, and no guess at the next.
        status.applyStatus(json(R"({"bank": {"banks": [["A", "B"]], "bank": 0, "position": -1, "size": 2}})"));
        QCOMPARE(status.position(), -1);
        QVERIFY(status.next().isEmpty());

        status.setUnreachable();
        QVERIFY(!status.reachable());
        QCOMPARE(status.bank(), -1);
        QVERIFY(status.next().isEmpty());
    }

    void anUnknownLayoutOnTheCommandLineIsRefused()
    {
        QTemporaryDir dir;
        QProcess app;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        app.setProcessEnvironment(env);
        app.start(QStringLiteral(RIGPA_BINARY),
                  {QStringLiteral("--config"), dir.filePath(QStringLiteral("none.json")),
                   QStringLiteral("--layout"), QStringLiteral("tiny")});
        QVERIFY(app.waitForFinished(15000));
        QCOMPARE(app.exitCode(), 2);
        QVERIFY(QString::fromUtf8(app.readAllStandardError()).contains(QStringLiteral("display.layout")));
    }
};

QTEST_MAIN(TestLayout)
#include "tst_layout.moc"
