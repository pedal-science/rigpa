#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include <optional>

namespace rigpa {

// A chain laid out like wrapped text: which row each tile lands on, where it sits, and the wire
// that carries the signal from the end of one row to the start of the next. A port of ljoss's
// FlowArrangement (DragEngine/FlowGeometry.swift), and pure, so its rules are tested without a
// window (tests/tst_flow.cpp ports ljoss's Tests/FlowTests.swift).
struct FlowArrangement {
    struct Style {
        qreal rowSpacing = 32;   // the return wire runs through the middle of the gap
        qreal wireThickness = 3;
        qreal stubLength = 14;   // how far the wire runs right of a row's last tile
        // Kept clear at the right of every row so the out stub never runs past the wrap width.
        qreal trailingReserve() const { return stubLength + wireThickness; }
    };

    struct Row {
        QList<int> items;  // left to right
        QRectF frame;      // from x = 0 to the end of the last item; as tall as its tallest
    };

    // The four straight runs of wire across one line break, as rects that meet edge to edge.
    enum WirePiece { OutStub, RightDrop, ReturnRun, LeftDrop, WirePieceCount };

    QList<QRectF> itemFrames;   // one per item, top-left in the layout's own space
    QList<Row> rows;
    QList<QList<QRectF>> wires; // one per line break, indexed by WirePiece
    QSizeF size;
    QRectF restingWire;         // zero-sized at the end of the last row

    // No wrap width keeps everything on one line.
    FlowArrangement(const QList<QSizeF> &sizes, std::optional<qreal> wrapWidth, const Style &style);
};

// Which slot a pointer is aiming at in a container whose tiles may be wrapped onto several rows
// (ljoss's FlowHitTest).
namespace flowhit {

struct Mark {
    QRectF rect;
    int before; // the slot meant left of this mark's midpoint
    int after;  // the slot meant right of it, with nothing further along the row

    static Mark element(int index, const QRectF &r) { return {r, index, index + 1}; }
    // A fixed tile both sides of which lead to one slot: Input (0) or Output (count).
    static Mark endpoint(int slot, const QRectF &r) { return {r, slot, slot}; }
};

int insertionIndex(const QList<Mark> &marks, int count, const QPointF &point);

} // namespace flowhit
} // namespace rigpa
