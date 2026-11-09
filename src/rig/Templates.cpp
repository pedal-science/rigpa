#include "Templates.h"

#include "pedald/PedaldClient.h"
#include "pedald/PedaldEvents.h"

#include <QJsonArray>

namespace rigpa {

Templates::Templates(QObject *parent) : QObject(parent) {}

void Templates::follow(PedaldClient *pedald, PedaldEvents *events)
{
    m_pedald = pedald;
    connect(events, &PedaldEvents::event, this, [this](const QString &kind, const QJsonObject &) {
        if (kind == QLatin1String("hello"))
            read();
    });
    read();
}

void Templates::read()
{
    if (!m_pedald || !m_pedald->configured())
        return;
    m_pedald->get(QStringLiteral("/catalogue"), [this](const PedaldReply &reply) {
        if (reply.ok)
            apply(reply.body.object());
    });
}

void Templates::apply(const QJsonObject &catalogue)
{
    QList<Template> templates;
    for (const auto &t : catalogue.value(QStringLiteral("templates")).toArray()) {
        const QJsonObject o = t.toObject();
        Template tpl;
        tpl.id = o.value(QStringLiteral("id")).toString();
        tpl.name = o.value(QStringLiteral("name")).toString();
        tpl.subtitle = o.value(QStringLiteral("subtitle")).toString();
        tpl.category = o.value(QStringLiteral("category")).toString();
        tpl.symbol = o.value(QStringLiteral("symbol")).toString();
        for (const auto &s : o.value(QStringLiteral("sources")).toArray())
            tpl.sources.append(s.toString());
        for (const auto &n : o.value(QStringLiteral("nodes")).toArray()) {
            const QJsonObject no = n.toObject();
            tpl.nodes.append({no.value(QStringLiteral("slot")).toString(), no.value(QStringLiteral("type")).toString(),
                              no.value(QStringLiteral("fixed")).toObject(), no.value(QStringLiteral("assets")).toObject()});
        }
        for (const auto &c : o.value(QStringLiteral("controls")).toArray()) {
            const QJsonObject co = c.toObject();
            Control control;
            control.id = co.value(QStringLiteral("id")).toString();
            control.name = co.value(QStringLiteral("name")).toString();
            control.kind = co.value(QStringLiteral("kind")).toString();
            control.min = co.value(QStringLiteral("min")).toDouble(0);
            control.max = co.value(QStringLiteral("max")).toDouble(10);
            control.step = co.value(QStringLiteral("step")).toDouble(0);
            control.unit = co.value(QStringLiteral("unit")).toString();
            control.slot = co.value(QStringLiteral("slot")).toString();
            control.param = co.value(QStringLiteral("param")).toString();
            control.transform = co.value(QStringLiteral("transform")).toObject();
            if (control.kind == QLatin1String("options")) {
                for (const auto &option : co.value(QStringLiteral("options")).toArray())
                    control.options.append(option.toString());
                control.defaultOption = co.value(QStringLiteral("default")).toString();
            } else {
                control.defaultValue = co.value(QStringLiteral("default")).toDouble();
            }
            tpl.controls.append(control);
        }
        if (!tpl.id.isEmpty())
            templates.append(tpl);
    }
    m_templates = templates;
    emit changed();
}

const Templates::Template *Templates::find(const QString &id) const
{
    for (const Template &t : m_templates)
        if (t.id == id)
            return &t;
    return nullptr;
}

const Templates::Template *Templates::forNodes(const QList<QJsonObject> &nodes) const
{
    if (nodes.isEmpty())
        return nullptr;
    return find(nodes.first().value(QStringLiteral("meta")).toObject().value(QStringLiteral("template")).toString());
}

QJsonValue Templates::toEngine(const QJsonObject &transform, const QJsonValue &value)
{
    const QString kind = transform.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("option"))
        return transform.value(QStringLiteral("map")).toObject().value(value.toString());
    const double v = value.toDouble();
    if (kind == QLatin1String("knob_decibels"))
        return (v - 5) / 5 * transform.value(QStringLiteral("span")).toDouble();
    if (kind == QLatin1String("knob_fraction"))
        return v / 10 * transform.value(QStringLiteral("top")).toDouble();
    if (kind == QLatin1String("knob_attenuation")) {
        const double floor = transform.value(QStringLiteral("floor")).toDouble();
        return floor - v / 10 * floor;
    }
    if (kind == QLatin1String("multiplied"))
        return v * transform.value(QStringLiteral("by")).toDouble();
    return v;
}

QJsonValue Templates::fromEngine(const QJsonObject &transform, const QJsonValue &value)
{
    const QString kind = transform.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("option")) {
        const QJsonObject map = transform.value(QStringLiteral("map")).toObject();
        const QString engine = value.isDouble() ? QString::number(value.toDouble()) : value.toString();
        for (auto it = map.begin(); it != map.end(); ++it)
            if (it.value().toString() == engine)
                return it.key();
        return QJsonValue();
    }
    const double v = value.toDouble();
    if (kind == QLatin1String("knob_decibels"))
        return v / transform.value(QStringLiteral("span")).toDouble() * 5 + 5;
    if (kind == QLatin1String("knob_fraction"))
        return v / transform.value(QStringLiteral("top")).toDouble() * 10;
    if (kind == QLatin1String("knob_attenuation")) {
        const double floor = transform.value(QStringLiteral("floor")).toDouble();
        return (floor - v) / floor * 10;
    }
    if (kind == QLatin1String("multiplied"))
        return v / transform.value(QStringLiteral("by")).toDouble();
    return v;
}

} // namespace rigpa
