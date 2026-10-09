#include "Flow.h"

#include <algorithm>
#include <limits>

namespace rigpa {

FlowArrangement::FlowArrangement(const QList<QSizeF> &sizes, std::optional<qreal> wrapWidth,
                                 const Style &style)
{
    // Line breaking: break before an item that would overrun the row, except an item with no
    // width (a closed drop gap, which fits anywhere and so must never start a row), and except on
    // a row still empty, where an item wider than the limit overflows.
    std::optional<qreal> limit;
    if (wrapWidth)
        limit = *wrapWidth - style.trailingReserve();
    QList<QList<int>> grouped{{}};
    qreal x = 0;
    for (int i = 0; i < sizes.size(); ++i) {
        const QSizeF &s = sizes.at(i);
        if (limit && s.width() > 0 && x > 0 && x + s.width() > *limit) {
            grouped.append(QList<int>());
            x = 0;
        }
        grouped.last().append(i);
        x += s.width();
    }

    // Placement: items on a row share its vertical centre, which is where the wire runs.
    itemFrames = QList<QRectF>(sizes.size());
    qreal y = 0;
    for (const QList<int> &items : grouped) {
        qreal height = 0;
        for (int i : items)
            height = std::max(height, sizes.at(i).height());
        qreal cursor = 0;
        for (int i : items) {
            const QSizeF &s = sizes.at(i);
            itemFrames[i] = QRectF(cursor, y + (height - s.height()) / 2, s.width(), s.height());
            cursor += s.width();
        }
        rows.append({items, QRectF(0, y, cursor, height)});
        y += height + style.rowSpacing;
    }

    // The return wire: right out of the last tile, down past the row, back across the gutter,
    // and down into the next row from the left. The next row's first tile carries its own
    // leading wire, which completes the run.
    const qreal t = style.wireThickness;
    const qreal h = t / 2;
    const qreal s = style.stubLength;
    for (int r = 0; r + 1 < rows.size(); ++r) {
        const QRectF above = rows.at(r).frame;
        const QRectF below = rows.at(r + 1).frame;
        const qreal right = above.right();
        const qreal left = 0;
        const qreal y0 = above.center().y();
        const qreal run = above.bottom() + style.rowSpacing / 2;
        const qreal y1 = below.center().y();
        QList<QRectF> pieces(WirePieceCount);
        pieces[OutStub] = QRectF(right, y0 - h, s + h, t);
        pieces[RightDrop] = QRectF(right + s - h, y0 + h, t, run - y0);
        pieces[ReturnRun] = QRectF(left - t, run - h, right + s - h - (left - t), t);
        pieces[LeftDrop] = QRectF(left - t, run + h, t, y1 - run);
        wires.append(pieces);
    }

    qreal widest = 0;
    for (const Row &row : rows)
        widest = std::max(widest, row.frame.width());
    size = QSizeF(widest + (rows.size() > 1 ? style.trailingReserve() : 0),
                  std::max<qreal>(0, y - style.rowSpacing));
    const QRectF last = rows.last().frame;
    restingWire = QRectF(last.right(), last.center().y(), 0, 0);
}

namespace flowhit {

int insertionIndex(const QList<Mark> &marks, int count, const QPointF &point)
{
    if (marks.isEmpty())
        return count;

    // Group into rows: tiles on one row overlap vertically, because they are centred on the
    // same wire; a tile wholly below the row so far starts the next. (Comparing x instead breaks
    // as soon as an open gap pushes a row's first tile right.)
    QList<QList<Mark>> rows;
    qreal bottom = -std::numeric_limits<qreal>::infinity();
    for (const Mark &mark : marks) {
        if (rows.isEmpty() || mark.rect.top() > bottom) {
            rows.append({mark});
            bottom = mark.rect.bottom();
        } else {
            rows.last().append(mark);
            bottom = std::max(bottom, mark.rect.bottom());
        }
    }

    // The pointer's row: the space between two rows splits at its midpoint.
    const QList<Mark> *row = &rows.last();
    for (int r = 0; r + 1 < rows.size(); ++r) {
        qreal aboveBottom = 0, belowTop = std::numeric_limits<qreal>::infinity();
        for (const Mark &m : rows.at(r))
            aboveBottom = std::max(aboveBottom, m.rect.bottom());
        for (const Mark &m : rows.at(r + 1))
            belowTop = std::min(belowTop, m.rect.top());
        if (point.y() < (aboveBottom + belowTop) / 2) {
            row = &rows.at(r);
            break;
        }
    }

    // Within the row, the first tile whose midpoint is right of the pointer.
    for (const Mark &mark : *row)
        if (point.x() < mark.rect.center().x())
            return std::min(mark.before, count);
    return std::min(row->last().after, count);
}

} // namespace flowhit
} // namespace rigpa
