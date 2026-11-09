#include "PaletteModel.h"

#include "RigEdits.h"
#include "Templates.h"
#include "engine/EngineSession.h"
#include "library/Library.h"

namespace rigpa {

namespace {

// The part of a node id after its element, as ljoss writes them.
QString suffixFor(const QString &type)
{
    if (type == QLatin1String("nc.nam"))
        return QStringLiteral("nam");
    if (type == QLatin1String("fx.amp.tonestack"))
        return QStringLiteral("tone");
    if (type.endsWith(QLatin1String(".ir")))
        return QStringLiteral("cab");
    return type.section(u'.', -1);
}

bool isPlumbing(const QString &type)
{
    static const QStringList plumbing{"nc.input", "nc.output", "nc.split", "nc.mix", "nc.bypass", "nc.unit_delay"};
    return plumbing.contains(type);
}

} // namespace

PaletteModel::PaletteModel(EngineSession *engine, Library *library, QObject *parent)
    : QAbstractListModel(parent), m_engine(engine), m_library(library)
{
    connect(&m_ring, &FocusRing::activated, this, &PaletteModel::choose);
    connect(m_engine, &EngineSession::schemaChanged, this, &PaletteModel::rebuild);
    rebuild();
}

void PaletteModel::setTemplates(Templates *templates)
{
    m_templates = templates;
    connect(templates, &Templates::changed, this, &PaletteModel::rebuild);
    rebuild();
}

void PaletteModel::rebuild()
{
    beginResetModel();
    m_entries.clear();
    const bool templates = m_templates && !m_templates->all().isEmpty();
    if (templates) {
        for (const Templates::Template &t : m_templates->all()) {
            QString category = t.category == QLatin1String("eq") ? QStringLiteral("EQ") : t.category;
            if (!category.isEmpty())
                category[0] = category[0].toUpper();
            m_entries.append({QStringLiteral("template"), t.id, t.name, category + QStringLiteral(" · ") + t.subtitle});
        }
    } else {
        m_entries.append({QStringLiteral("amp"), {}, tr("Amp"), tr("A NAM model and a tone stack")});
    }
    m_entries.append({QStringLiteral("section"), {}, tr("Parallel split"), tr("Two lanes, mixed back together")});
    const Schema &schema = m_engine->schema();
    for (const QString &id : schema.typeIds()) {
        if (isPlumbing(id))
            continue;
        const NodeTypeSpec *t = schema.type(id);
        if (t != nullptr && !t->inPalette)
            continue; // not a block: a client mounts it around a view (nc.tuner)
        QString detail = t->category == QLatin1String("eq") ? QStringLiteral("EQ") : t->category;
        if (!detail.isEmpty())
            detail[0] = detail[0].toUpper();
        m_entries.append({QStringLiteral("type"), id, t->name, detail + (t->macro ? QString() : QStringLiteral(" · ") + tr("a single node"))});
    }
    endResetModel();
    m_ring.setCount(int(m_entries.size()));
}

void PaletteModel::open()
{
    m_open = true;
    m_ring.setKinds({});
    m_ring.setCount(int(m_entries.size()));
    m_ring.setHighlight(0);
    emit openChanged();
}

void PaletteModel::close()
{
    if (!m_open)
        return;
    m_open = false;
    emit openChanged();
}

void PaletteModel::choose(int row)
{
    if (!m_open || row < 0 || row >= m_entries.size())
        return;
    close();
    emit chosen(row);
}

int PaletteModel::indexOfType(const QString &type) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries.at(i).type == type || m_entries.at(i).kind == type)
            return i;
    return -1;
}

std::optional<RigNode> PaletteModel::node(const QString &element, const QString &type, QString *why) const
{
    QJsonObject params;
    return node(element, type, suffixFor(type), params, why);
}

std::optional<RigNode> PaletteModel::node(const QString &element, const QString &type, const QString &suffix,
                                          QJsonObject params, QString *why) const
{
    if (const NodeTypeSpec *t = m_engine->schema().type(type)) {
        for (const ParamSpec &p : t->params) {
            if (p.optionsFrom.isEmpty())
                continue;
            // A model or IR must name something: the library's first, changed in the editor.
            const auto kind = p.optionsFrom == QLatin1String("irs") ? LibraryEntry::Ir : LibraryEntry::Model;
            const auto &entries = m_library ? m_library->entries(kind) : QList<LibraryEntry>();
            if (entries.isEmpty()) {
                if (why)
                    *why = kind == LibraryEntry::Ir ? tr("There are no IRs in the library") : tr("There are no models in the library");
                return std::nullopt;
            }
            if (!params.contains(p.id))
                params.insert(p.id, entries.first().id);
        }
    }
    return edits::newNode(element, suffix, type, params);
}

std::optional<RigElement> PaletteModel::make(int row, QString *why) const
{
    if (row < 0 || row >= m_entries.size())
        return std::nullopt;
    const Entry &e = m_entries.at(row);
    if (e.kind == QLatin1String("section"))
        return edits::newSection(2);
    RigElement block;
    block.element = edits::newElementId();
    if (e.kind == QLatin1String("template")) {
        const Templates::Template *tpl = m_templates ? m_templates->find(e.type) : nullptr;
        if (!tpl)
            return std::nullopt;
        for (const Templates::Node &n : tpl->nodes) {
            // Its fixed values, each control at its default as the engine's value, and where it came
            // from, so every app shows it as this block (pedal-service.md, "The catalogue").
            QJsonObject params = n.fixed;
            for (const Templates::Control &c : tpl->controls) {
                if (c.slot != n.slot)
                    continue;
                const QJsonValue value = c.kind == QLatin1String("options") ? QJsonValue(c.defaultOption) : QJsonValue(c.defaultValue);
                params.insert(c.param, Templates::toEngine(c.transform, value));
            }
            params.insert(QStringLiteral("meta"), QJsonObject{{QStringLiteral("template"), tpl->id}, {QStringLiteral("slot"), n.slot}});
            const auto made = node(block.element, n.type, n.slot, params, why);
            if (!made)
                return std::nullopt;
            block.nodes.append(*made);
        }
        return block;
    }
    const QStringList types = e.kind == QLatin1String("amp")
                                  ? QStringList{QStringLiteral("nc.nam"), QStringLiteral("fx.amp.tonestack")}
                                  : QStringList{e.type};
    for (const QString &type : types) {
        const auto n = node(block.element, type, why);
        if (!n)
            return std::nullopt;
        block.nodes.append(*n);
    }
    return block;
}

int PaletteModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant PaletteModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry &e = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return e.name;
    case DetailRole:
        return e.detail;
    case TintRole:
        return e.kind;
    }
    return {};
}

QHash<int, QByteArray> PaletteModel::roleNames() const
{
    return {{NameRole, "name"}, {DetailRole, "detail"}, {TintRole, "entryKind"}};
}

} // namespace rigpa
