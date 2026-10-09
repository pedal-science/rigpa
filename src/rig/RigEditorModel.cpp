#include "RigEditorModel.h"

#include "RigSession.h"
#include "library/Library.h"

#include <QHash>

#include <algorithm>

namespace rigpa {

namespace {

const QString kNam = QStringLiteral("nc.nam");

bool isCab(const QString &type)
{
    return type == QLatin1String("fx.cab.ir") || type == QLatin1String("nc.cab.ir");
}

QString leaf(const QString &id)
{
    return id.section(u'/', -1);
}

// A colour per kind of processing, so a rig reads at a glance.
QString colorFor(const QString &type)
{
    if (type == kNam)
        return QStringLiteral("#e08050");
    if (isCab(type))
        return QStringLiteral("#80b0d0");
    if (type.startsWith(QLatin1String("fx.eq")) || type == QLatin1String("nc.eq.biquad")
        || type == QLatin1String("fx.filter.cut") || type == QLatin1String("fx.amp.tonestack"))
        return QStringLiteral("#7cc47c");
    if (type.contains(QLatin1String("delay")) || type == QLatin1String("fx.mix.drywet"))
        return QStringLiteral("#b08be0");
    return QStringLiteral("#9a9a9a");
}

} // namespace

QString RigEditorModel::typeLabel(const QString &type)
{
    static const QHash<QString, QString> labels = {
        {"nc.nam", "Amp"}, {"nc.cab.ir", "Cab IR"}, {"fx.cab.ir", "Cab IR"}, {"nc.gain", "Gain"},
        {"nc.eq.biquad", "Biquad EQ"}, {"nc.delay", "Delay line"}, {"fx.eq.bands", "EQ"},
        {"fx.eq.tilt", "Tilt EQ"}, {"fx.filter.cut", "Cut filter"}, {"fx.mix.drywet", "Dry/wet"},
        {"fx.delay.feedback", "Delay"}, {"fx.amp.tonestack", "Tone stack"}, {"nc.split", "Split"},
        {"nc.mix", "Mix"},
    };
    return labels.value(type, type);
}

RigEditorModel::RigEditorModel(RigSession *session, Library *library, QObject *parent)
    : QAbstractListModel(parent), m_session(session), m_library(library)
{
    if (m_session)
        connect(m_session, &RigSession::documentChanged, this, &RigEditorModel::relayout);
    if (m_library)
        connect(m_library, &Library::scanned, this, &RigEditorModel::relayout); // names arrive later
}

void RigEditorModel::setAvailableWidth(qreal width)
{
    if (qFuzzyCompare(width, m_availableWidth))
        return;
    m_availableWidth = width;
    emit availableWidthChanged();
    relayout();
}

void RigEditorModel::relayout()
{
    if (m_session && m_session->hasDocument())
        layout(m_session->document(), m_session->missingNodes());
    else
        layout({}, {});
}

QSizeF RigEditorModel::laneSize(const QList<RigElement> &lane) const
{
    if (lane.isEmpty())
        return {m_style.emptyLane, m_style.tileHeight};
    qreal w = 0, h = 0;
    for (const RigElement &e : lane) {
        const QSizeF s = elementSize(e);
        w += m_style.connector + s.width();
        h = std::max(h, s.height());
    }
    return {w + m_style.connector, h}; // and a trailing wire into the mix
}

QSizeF RigEditorModel::elementSize(const RigElement &e) const
{
    if (e.kind == RigElement::Block)
        return {m_style.tileWidth, m_style.tileHeight};
    qreal w = 0, h = 0;
    for (const auto &lane : e.lanes) {
        const QSizeF s = laneSize(lane);
        w = std::max(w, s.width());
        h += s.height();
    }
    h += m_style.laneGap * std::max<qsizetype>(0, e.lanes.size() - 1) + 2 * m_style.laneGap;
    return {w + 2 * m_style.bar, h};
}

void RigEditorModel::wire(const QRectF &r)
{
    m_wires.append(r);
}

void RigEditorModel::describe(const RigElement &e, Item &item) const
{
    item.element = e.element;
    if (e.kind == RigElement::Section) {
        item.kind = QStringLiteral("section");
        item.title = tr("Parallel");
        item.subtitle = tr("%n lane(s)", nullptr, int(e.lanes.size()));
        item.color = QStringLiteral("#d0c060");
        item.bypassed = e.split.bypass;
        return;
    }
    item.kind = QStringLiteral("block");
    QStringList kinds;
    QString title, brand;
    bool allBypassed = !e.nodes.isEmpty();
    for (const RigNode &n : e.nodes) {
        allBypassed = allBypassed && n.bypass;
        item.missing = item.missing || m_missing.contains(n.id);
        if (n.type == kNam && title.isEmpty()) {
            const QString id = n.json.value(QStringLiteral("model")).toString();
            const LibraryEntry *entry = m_library ? m_library->find(LibraryEntry::Model, id) : nullptr;
            title = entry ? entry->name : leaf(id);
            if (entry && entry->make != QLatin1String("Unknown"))
                brand = entry->make;
        } else if (isCab(n.type) && title.isEmpty()) {
            const QString id = n.json.value(QStringLiteral("ir")).toString();
            const LibraryEntry *entry = m_library ? m_library->find(LibraryEntry::Ir, id) : nullptr;
            title = entry ? entry->name : leaf(id);
        }
        kinds.append(typeLabel(n.type));
    }
    kinds.removeDuplicates();
    if (title.isEmpty()) {
        title = kinds.value(0);
        kinds.removeFirst();
    }
    if (!brand.isEmpty())
        kinds.prepend(brand);
    item.title = title;
    item.subtitle = kinds.join(QStringLiteral(" · "));
    item.color = colorFor(e.nodes.isEmpty() ? QString() : e.nodes.first().type);
    item.bypassed = allBypassed;
}

void RigEditorModel::placeBlock(const RigElement &e, const QRectF &tile, int chain)
{
    Item item;
    describe(e, item);
    item.rect = tile;
    item.chain = chain;
    item.select = m_selectable++;
    m_items.append(item);
}

void RigEditorModel::placeSection(const RigElement &e, QPointF origin, int chain)
{
    const QSizeF size = elementSize(e);
    Item frame;
    describe(e, frame);
    frame.rect = QRectF(origin, size);
    frame.chain = chain;
    frame.select = m_selectable++;
    m_items.append(frame);

    const qreal t = m_style.wire, half = t / 2;
    const qreal splitX = origin.x(), mixX = origin.x() + size.width() - m_style.bar;
    qreal y = origin.y() + m_style.laneGap;
    qreal firstMid = -1, lastMid = -1;
    for (const auto &lane : e.lanes) {
        const QSizeF ls = laneSize(lane);
        const qreal mid = y + ls.height() / 2;
        if (firstMid < 0)
            firstMid = mid;
        lastMid = mid;
        qreal x = splitX + m_style.bar;
        for (const RigElement &el : lane) {
            const QSizeF s = elementSize(el);
            wire(QRectF(x, mid - half, m_style.connector, t));
            x += m_style.connector;
            if (el.kind == RigElement::Block)
                placeBlock(el, QRectF(x, mid - s.height() / 2, s.width(), s.height()), chain);
            else
                placeSection(el, QPointF(x, mid - s.height() / 2), chain);
            x += s.width();
        }
        wire(QRectF(x, mid - half, mixX - x, t)); // on to the mix (straight across when empty)
        y += ls.height() + m_style.laneGap;
    }
    // The split and mix bars, joining the lanes' wires.
    wire(QRectF(splitX, firstMid - half, m_style.bar, lastMid - firstMid + t));
    wire(QRectF(mixX, firstMid - half, m_style.bar, lastMid - firstMid + t));
}

void RigEditorModel::layout(const RigDocument &doc, const QSet<QString> &missing)
{
    beginResetModel();
    m_items.clear();
    m_wires.clear();
    m_selectable = 0;
    m_missing = missing;
    const Style &st = m_style;
    const FlowArrangement::Style flowStyle{st.rowSpacing, st.wire, st.connector};
    const qreal wrap = std::max<qreal>(m_availableWidth - 2 * st.padding, st.ioWidth + st.connector + st.tileWidth);

    qreal y = st.padding;
    qreal widest = 0;
    for (int c = 0; c < doc.chains.size(); ++c) {
        const RigChain &chain = doc.chains.at(c);
        Item label;
        label.kind = QStringLiteral("label");
        label.chain = c;
        label.title = chain.name.isEmpty() ? tr("Chain %1").arg(c + 1) : chain.name;
        label.subtitle = tr("in %1 → out %2").arg(chain.inputChannel + 1).arg(chain.outputChannel + 1);
        if (!chain.editable)
            label.subtitle += QStringLiteral(" · ") + tr("shown as written: %1").arg(chain.problem);
        label.rect = QRectF(st.padding, y, wrap, st.chainLabel - 6);
        m_items.append(label);
        y += st.chainLabel;

        QList<QSizeF> sizes{{st.ioWidth, st.tileHeight}};
        for (const RigElement &e : chain.series) {
            const QSizeF s = elementSize(e);
            sizes.append({st.connector + s.width(), s.height()});
        }
        sizes.append({st.connector + st.ioWidth, st.tileHeight});
        const FlowArrangement flow(sizes, wrap, flowStyle);
        const QPointF at(st.padding, y);

        auto io = [&](const QRectF &r, bool input) {
            Item item;
            item.kind = QStringLiteral("io");
            item.rect = r;
            item.chain = c;
            item.title = input ? tr("In %1").arg(chain.inputChannel + 1) : tr("Out %1").arg(chain.outputChannel + 1);
            item.color = QStringLiteral("#5a5a5a");
            m_items.append(item);
        };
        io(flow.itemFrames.first().translated(at), true);
        for (int i = 0; i < chain.series.size(); ++i) {
            const QRectF f = flow.itemFrames.at(i + 1).translated(at);
            const RigElement &e = chain.series.at(i);
            wire(QRectF(f.left(), f.center().y() - st.wire / 2, st.connector, st.wire));
            const QRectF body(f.left() + st.connector, f.top(), f.width() - st.connector, f.height());
            if (e.kind == RigElement::Block)
                placeBlock(e, body, c);
            else
                placeSection(e, body.topLeft(), c);
        }
        const QRectF out = flow.itemFrames.last().translated(at);
        wire(QRectF(out.left(), out.center().y() - st.wire / 2, st.connector, st.wire));
        io(QRectF(out.left() + st.connector, out.top(), st.ioWidth, out.height()), false);
        for (const auto &pieces : flow.wires)
            for (const QRectF &p : pieces)
                wire(p.translated(at));

        widest = std::max(widest, flow.size.width());
        y += flow.size.height() + st.chainGap;
    }
    m_contentWidth = widest + 2 * st.padding;
    m_contentHeight = doc.chains.isEmpty() ? 0 : y - st.chainGap + st.padding;
    endResetModel();
    emit laidOut();
}

QString RigEditorModel::elementAt(int select) const
{
    for (const Item &i : m_items)
        if (i.select == select)
            return i.element;
    return {};
}

int RigEditorModel::selectOf(const QString &element) const
{
    for (const Item &i : m_items)
        if (i.select >= 0 && i.element == element)
            return i.select;
    return -1;
}

QRectF RigEditorModel::rectOf(int select) const
{
    for (const Item &i : m_items)
        if (i.select == select)
            return i.rect;
    return {};
}

int RigEditorModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant RigEditorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &i = m_items.at(index.row());
    switch (role) {
    case KindRole: return i.kind;
    case XRole: return i.rect.x();
    case YRole: return i.rect.y();
    case WRole: return i.rect.width();
    case HRole: return i.rect.height();
    case TitleRole: return i.title;
    case SubtitleRole: return i.subtitle;
    case ColorRole: return i.color;
    case BypassedRole: return i.bypassed;
    case MissingRole: return i.missing;
    case SelectRole: return i.select;
    case ElementRole: return i.element;
    case ChainRole: return i.chain;
    }
    return {};
}

QHash<int, QByteArray> RigEditorModel::roleNames() const
{
    return {{KindRole, "kind"}, {XRole, "itemX"}, {YRole, "itemY"}, {WRole, "itemW"}, {HRole, "itemH"},
            {TitleRole, "title"}, {SubtitleRole, "subtitle"}, {ColorRole, "tint"}, {BypassedRole, "bypassed"},
            {MissingRole, "missing"}, {SelectRole, "select"}, {ElementRole, "element"}, {ChainRole, "chain"}};
}

} // namespace rigpa
