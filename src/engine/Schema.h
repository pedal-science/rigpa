#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace rigpa {

// One parameter of a node type, as `engine.schema` publishes it (namchain docs/schema.md).
struct ParamSpec {
    enum Kind { Number, Option, Flag, Other };
    QString id;
    QString name;
    Kind kind = Number;
    std::optional<double> min, max;
    bool minInclusive = true;    // whether the bound itself is allowed ("min_inclusive")
    bool maxInclusive = true;
    double defaultValue = 0;
    bool hasDefault = false;
    bool log = false;            // "scale": "log"
    double step = 0;             // 0: continuous
    QString unit;
    bool hidden = false;
    bool rampable = false;
    bool patchable = true;
    QString oneOf;               // a group of alternatives (nc.delay's samples | ms); "" when none
    struct Choice {
        QString label;
        QString value;
    };
    QList<Choice> options;
    QString optionsFrom;         // "models" or "irs": a library reference, chosen with the picker
};

struct NodeTypeSpec {
    QString id;
    QString name;
    QString category;
    bool macro = false;
    bool bypassable = false;
    QList<ParamSpec> params;

    const ParamSpec *param(const QString &id) const;
};

// The engine's catalogue of node types, fetched once per connection.
class Schema
{
public:
    static Schema parse(const QJsonObject &reply);

    bool isEmpty() const { return m_types.isEmpty(); }
    const NodeTypeSpec *type(const QString &id) const;
    QStringList typeIds() const { return m_order; }

    // A number parameter's range, with a stand-in where the schema leaves an end open (a biquad's
    // frequency has no maximum): what a knob needs to have two ends. Both ends are values the engine
    // accepts: an excluded bound is approached by a step (or a ten-thousandth, rigpa's finest), and a
    // logarithmic parameter's end at zero is a stand-in a thousandth of the other end.
    static double lowerOf(const ParamSpec &p);
    static double upperOf(const ParamSpec &p);

private:
    QHash<QString, NodeTypeSpec> m_types;
    QStringList m_order;
};

} // namespace rigpa
