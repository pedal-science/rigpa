#include "Schema.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>

namespace rigpa {

namespace {

ParamSpec::Kind kindOf(const QString &kind)
{
    if (kind == QLatin1String("number"))
        return ParamSpec::Number;
    if (kind == QLatin1String("option"))
        return ParamSpec::Option;
    if (kind == QLatin1String("flag"))
        return ParamSpec::Flag;
    return ParamSpec::Other;
}

std::optional<double> numberOrNothing(const QJsonValue &v)
{
    if (v.isDouble())
        return v.toDouble();
    return std::nullopt;
}

ParamSpec parseParam(const QJsonObject &o)
{
    ParamSpec p;
    p.id = o.value(QStringLiteral("id")).toString();
    p.name = o.value(QStringLiteral("name")).toString(p.id);
    p.kind = kindOf(o.value(QStringLiteral("kind")).toString());
    p.min = numberOrNothing(o.value(QStringLiteral("min")));
    p.max = numberOrNothing(o.value(QStringLiteral("max")));
    p.minInclusive = o.value(QStringLiteral("min_inclusive")).toBool(true);
    p.maxInclusive = o.value(QStringLiteral("max_inclusive")).toBool(true);
    const QJsonValue def = o.value(QStringLiteral("default"));
    p.hasDefault = def.isDouble() || def.isBool();
    p.defaultValue = def.isBool() ? (def.toBool() ? 1 : 0) : def.toDouble();
    p.log = o.value(QStringLiteral("scale")).toString() == QLatin1String("log");
    p.step = o.value(QStringLiteral("step")).toDouble(0);
    if (p.step <= 0 && o.value(QStringLiteral("snap")).toBool())
        p.step = 1;
    p.unit = o.value(QStringLiteral("unit")).toString();
    p.hidden = o.value(QStringLiteral("hidden")).toBool();
    p.rampable = o.value(QStringLiteral("rampable")).toBool();
    p.patchable = o.value(QStringLiteral("patchable")).toBool(true);
    p.oneOf = o.value(QStringLiteral("one_of")).toString();
    p.optionsFrom = o.value(QStringLiteral("options_from")).toString();
    // A library reference's options are the whole library; rigpa reads that itself.
    if (p.optionsFrom.isEmpty()) {
        for (const auto &c : o.value(QStringLiteral("options")).toArray()) {
            const QJsonObject choice = c.toObject();
            p.options.append({choice.value(QStringLiteral("label")).toString(),
                              choice.value(QStringLiteral("value")).toVariant().toString()});
        }
    }
    return p;
}

} // namespace

const ParamSpec *NodeTypeSpec::param(const QString &id) const
{
    for (const ParamSpec &p : params)
        if (p.id == id)
            return &p;
    return nullptr;
}

Schema Schema::parse(const QJsonObject &reply)
{
    Schema schema;
    for (const auto &n : reply.value(QStringLiteral("nodes")).toArray()) {
        const QJsonObject o = n.toObject();
        NodeTypeSpec t;
        t.id = o.value(QStringLiteral("id")).toString();
        if (t.id.isEmpty())
            continue;
        t.name = o.value(QStringLiteral("name")).toString(t.id);
        t.category = o.value(QStringLiteral("category")).toString();
        t.macro = o.value(QStringLiteral("kind")).toString() == QLatin1String("macro");
        // A macro declares how it bypasses (docs/macros.md); a primitive says whether it can.
        t.bypassable = t.macro ? !o.value(QStringLiteral("bypass")).toString().isEmpty()
                               : o.value(QStringLiteral("flags")).toObject().value(QStringLiteral("bypassable")).toBool();
        for (const auto &p : o.value(QStringLiteral("params")).toArray())
            t.params.append(parseParam(p.toObject()));
        schema.m_order.append(t.id);
        schema.m_types.insert(t.id, t);
    }
    return schema;
}

const NodeTypeSpec *Schema::type(const QString &id) const
{
    const auto it = m_types.constFind(id);
    return it == m_types.cend() ? nullptr : &*it;
}

namespace {

// The nearest value inside an excluded bound: a step in, or a ten-thousandth (NodeEditorModel rounds
// continuous values to 1e-4, so a smaller nudge would round back onto the bound).
double inside(const ParamSpec &p, double bound, double direction)
{
    const double by = p.step > 0 ? p.step : std::max(std::abs(bound) * 1e-4, 1e-4);
    return bound + direction * by;
}

// Open-ended at the top: what the parameter's unit makes sensible on a guitar pedal.
double openUpper(const ParamSpec &p, double lower)
{
    if (p.unit == QLatin1String("Hz"))
        return 20000;
    if (p.unit == QLatin1String("ms"))
        return 2000;
    if (p.unit == QLatin1String("samples"))
        return 96000;
    return lower + 100;
}

} // namespace

double Schema::lowerOf(const ParamSpec &p)
{
    if (!p.min)
        return p.log ? 1 : 0;
    // A log scale can't reach zero (a Q has no lower end but "more than 0").
    if (p.log && *p.min <= 0)
        return (p.max ? *p.max : openUpper(p, 1)) / 1000;
    return p.minInclusive ? *p.min : inside(p, *p.min, +1);
}

double Schema::upperOf(const ParamSpec &p)
{
    if (p.max)
        return p.maxInclusive ? *p.max : inside(p, *p.max, -1);
    return openUpper(p, lowerOf(p));
}

} // namespace rigpa
