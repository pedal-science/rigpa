#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>

namespace rigpa {

class PedaldClient;
class PedaldEvents;

// The blocks the pedal offers (pedald's `GET /catalogue`, pedal.project docs/pedal-service.md, "The
// catalogue"): ljoss's 27 playable blocks, each a few engine nodes and the 0–10 controls a player
// turns, with how each control's number becomes the engine's and back. rigpa and ljoss offer the same
// blocks because both read them from the pedal.
//
// A block made from a template says so in its nodes' `meta`: `{"template": <id>, "slot": <slot>}`.
class Templates : public QObject
{
    Q_OBJECT

public:
    struct Node {
        QString slot, type;
        QJsonObject fixed;
        QJsonObject assets;   // parameter -> "model" or "ir"
    };
    struct Control {
        QString id, name, kind;   // kind: "continuous", "stepped" or "options"
        double min = 0, max = 10, step = 0, defaultValue = 0;
        QString defaultOption;
        QStringList options;
        QString unit, slot, param;
        QJsonObject transform;
    };
    struct Template {
        QString id, name, subtitle, category, symbol;
        QStringList sources;
        QList<Node> nodes;
        QList<Control> controls;
    };

    explicit Templates(QObject *parent = nullptr);
    // Reads the catalogue from pedald now and on each of its hellos.
    void follow(PedaldClient *pedald, PedaldEvents *events);
    // One `GET /catalogue` body (tests feed these directly).
    void apply(const QJsonObject &catalogue);

    const QList<Template> &all() const { return m_templates; }
    const Template *find(const QString &id) const;
    // The template a block came from, by its first node's `meta`, or nullptr.
    const Template *forNodes(const QList<QJsonObject> &nodes) const;

    // A control's value as the engine's, and the engine's as the control's (each inverts the other).
    static QJsonValue toEngine(const QJsonObject &transform, const QJsonValue &value);
    static QJsonValue fromEngine(const QJsonObject &transform, const QJsonValue &value);

signals:
    void changed();

private:
    void read();

    PedaldClient *m_pedald = nullptr;
    QList<Template> m_templates;
};

} // namespace rigpa
