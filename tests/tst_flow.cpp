// ljoss's Tests/FlowTests.swift, ported assertion for assertion: where rows break, the return
// wire, and which slot a pointer means once a chain spans several rows.
#include <QtTest>

#include "layout/Flow.h"

using namespace rigpa;

namespace {
const FlowArrangement::Style style{32, 3, 14};
const QSizeF tile(162, 104);
const QSizeF closedGap(0, 104);

QList<QList<int>> rowsOf(const FlowArrangement &f)
{
    QList<QList<int>> out;
    for (const auto &r : f.rows)
        out.append(r.items);
    return out;
}
} // namespace

class TestFlow : public QObject
{
    Q_OBJECT

private slots:
    void wrapping()
    {
        const QList<QSizeF> four(4, tile);
        const FlowArrangement line(four, std::nullopt, style);
        QCOMPARE(rowsOf(line), (QList<QList<int>>{{0, 1, 2, 3}}));
        QVERIFY(line.wires.isEmpty());
        QCOMPARE(line.size, QSizeF(648, 104)); // one line: no trailing reserve

        // 17 px reserve at the right: 2 × 162 = 324 fits in 341, 3 × 162 does not.
        const FlowArrangement wrapped(four, 358, style);
        QCOMPARE(rowsOf(wrapped), (QList<QList<int>>{{0, 1}, {2, 3}}));
        QCOMPARE(wrapped.itemFrames[2].topLeft(), QPointF(0, 136));
        QCOMPARE(wrapped.size, QSizeF(341, 240));

        const FlowArrangement exact(four, 324 + style.trailingReserve(), style);
        QCOMPARE(rowsOf(exact), (QList<QList<int>>{{0, 1}, {2, 3}}));

        // A closed gap must never start a row.
        const FlowArrangement gaps({tile, closedGap, tile, closedGap, tile}, 358, style);
        QCOMPARE(rowsOf(gaps), (QList<QList<int>>{{0, 1, 2, 3}, {4}}));

        // An open gap that doesn't fit opens at the start of the next row.
        const FlowArrangement open({tile, tile, QSizeF(162, 104), tile}, 358, style);
        QCOMPARE(rowsOf(open), (QList<QList<int>>{{0, 1}, {2, 3}}));

        const QSizeF wide(600, 236);
        const FlowArrangement over({tile, wide, tile}, 358, style);
        QCOMPARE(rowsOf(over), (QList<QList<int>>{{0}, {1}, {2}}));
        QCOMPARE(over.size.width(), 600 + style.trailingReserve());

        const FlowArrangement mixed({tile, wide}, std::nullopt, style);
        QCOMPARE(mixed.itemFrames[0].center().y(), mixed.itemFrames[1].center().y());
    }

    void returnWire()
    {
        const FlowArrangement flow(QList<QSizeF>(3, tile), 358, style);
        QCOMPARE(flow.wires.size(), 1);
        const auto &w = flow.wires[0];
        const QRectF out = w[FlowArrangement::OutStub], down = w[FlowArrangement::RightDrop];
        const QRectF run = w[FlowArrangement::ReturnRun], drop = w[FlowArrangement::LeftDrop];
        const QRectF above = flow.rows[0].frame, below = flow.rows[1].frame;

        QVERIFY(out.left() == above.right() && out.center().y() == above.center().y());
        QVERIFY(down.right() == out.right() && down.top() == out.bottom());
        QVERIFY(down.bottom() > above.bottom());
        QVERIFY(run.right() == down.left() && run.top() < down.bottom() && run.bottom() == down.bottom());
        QVERIFY(run.center().y() > above.bottom() && run.center().y() < below.top());
        QVERIFY(drop.left() == run.left() && drop.top() == run.bottom());
        QVERIFY(drop.right() == 0 && drop.bottom() == below.center().y() + 1.5);
        QVERIFY(out.right() <= 358);
        for (int i = 0; i < w.size(); ++i)
            for (int j = i + 1; j < w.size(); ++j) {
                const QRectF shared = w[i].intersected(w[j]);
                QVERIFY2(!(shared.width() > 0 && shared.height() > 0), "no two pieces overlap");
            }
    }

    void hitTesting()
    {
        const FlowArrangement flow(QList<QSizeF>(5, tile), 358, style);
        const auto &f = flow.itemFrames;
        QCOMPARE(rowsOf(flow), (QList<QList<int>>{{0, 1}, {2, 3}, {4}}));
        const QList<flowhit::Mark> marks{flowhit::Mark::endpoint(0, f[0]), flowhit::Mark::element(0, f[1]),
                                         flowhit::Mark::element(1, f[2]), flowhit::Mark::element(2, f[3]),
                                         flowhit::Mark::endpoint(3, f[4])};
        auto at = [&](qreal x, qreal y) { return flowhit::insertionIndex(marks, 3, {x, y}); };
        const qreal r0 = f[1].center().y(), r1 = f[2].center().y(), r2 = f[4].center().y();

        QCOMPARE(at(10, r0), 0);
        QCOMPARE(at(f[1].center().x() - 1, r0), 0);
        QCOMPARE(at(f[1].center().x() + 1, r0), 1);
        QCOMPARE(at(340, r0), 1);
        QCOMPARE(at(5, r1), 1);
        QCOMPARE(at(f[3].center().x() + 1, r1), 3);
        QCOMPARE(at(f[4].center().x() - 1, r2), 3);
        QCOMPARE(at(f[4].center().x() + 60, r2), 3);
        QCOMPARE(at(300, f[1].bottom() + 10), 1);
        QCOMPARE(at(10, f[2].top() - 10), 1);

        const QList<flowhit::Mark> pushed{flowhit::Mark::element(0, QRectF(0, 0, 162, 104)),
                                          flowhit::Mark::element(1, QRectF(176, 136, 162, 104))};
        QCOMPARE(flowhit::insertionIndex(pushed, 2, {100, 180}), 1);

        const QList<flowhit::Mark> single{flowhit::Mark::element(0, QRectF(0, 0, 100, 50)),
                                          flowhit::Mark::element(1, QRectF(120, 0, 100, 50))};
        QCOMPARE(flowhit::insertionIndex(single, 2, {60, 25}), 1);
        QCOMPARE(flowhit::insertionIndex(single, 2, {500, 25}), 2);
        QCOMPARE(flowhit::insertionIndex({}, 0, {}), 0);
    }
};

QTEST_GUILESS_MAIN(TestFlow)
#include "tst_flow.moc"
