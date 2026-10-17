#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

class EngineClient;

// The derived state of a running `nc.eq.flow` node, as namchain publishes it (`rig.state`,
// ../namchain/docs/floweq.md). flowEQ's node decodes three latent values into five EQ bands and
// hands back the magnitude response it built from them; drawing that curve is the difference
// between moving a knob and *seeing* what it did, and it is not something the UI can compute for
// itself without owning a copy of the model and the filter maths.
//
// The panel that shows this is open only while a flowEQ block is being edited, so the polling is
// bounded: one read when it opens, a fast one while a control is moving and for two seconds after,
// and nothing at all when the block is closed or the screen is idle (docs/spec.md, "the screen
// stays static when idle").
//
// What the engine answers is the *audible* plan's state, so a value seen here is a value that can
// be heard. The curve path is built here, in the engine's coordinates and this file's fixed chart
// size, because layout maths belongs in C++ and never in the QML (docs/spec.md, constraints).
class FlowEqModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns this")
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString curvePath READ curvePath NOTIFY changed)
    Q_PROPERTY(QString fillPath READ fillPath NOTIFY changed)
    Q_PROPERTY(int dbTop READ dbTop NOTIFY changed)
    Q_PROPERTY(int dbBottom READ dbBottom NOTIFY changed)
    Q_PROPERTY(int curveWidth READ curveWidth CONSTANT)
    Q_PROPERTY(int curveHeight READ curveHeight CONSTANT)
    Q_PROPERTY(QString gainText READ gainText NOTIFY changed)
    Q_PROPERTY(bool autoGain READ autoGain NOTIFY changed)
    Q_PROPERTY(QString modelName READ modelName NOTIFY changed)

public:
    // The chart's coordinate space. The pedal's screen is a fixed 1920x480 and the whole UI scales
    // rather than the panels, so one chart size is enough; the QML places it at its own x and y.
    static constexpr int kCurveWidth = 560;
    static constexpr int kCurveHeight = 236;
    static constexpr int kCurvePoints = 96;
    // The smallest half-range the chart shows, in dB: a curve that moves by a dB or two should be
    // visible rather than a flat line across a +/-24 dB plot.
    static constexpr int kMinRangeDb = 6;
    static constexpr int kMaxRangeDb = 48;
    // While a control is moving, and for this long after: how often the state is read.
    static constexpr int kPollMs = 100;
    static constexpr int kQuietMs = 2000;

    explicit FlowEqModel(QObject *parent = nullptr);

    void setClient(EngineClient *client) { m_client = client; }
    // Start reporting `element`'s node, or stop with an empty string. An empty string ends all
    // polling.
    void watch(const QString &element);
    QString watched() const { return m_element; }
    // A control moved (or a value was committed): read the state now, and keep reading for a while.
    void noteEdit();

    bool available() const { return m_available; }
    QString curvePath() const { return m_path; }
    QString fillPath() const { return m_fill; }
    int dbTop() const { return m_dbTop; }
    int dbBottom() const { return m_dbBottom; }
    int curveWidth() const { return kCurveWidth; }
    int curveHeight() const { return kCurveHeight; }
    QString gainText() const { return m_gainText; }
    bool autoGain() const { return m_autoGain; }
    QString modelName() const { return m_model; }

    // For tests: digest one `rig.state` reply body, as `EngineClient` would deliver it.
    void applyReply(const QJsonObject &body);

signals:
    void changed();

private:
    void request();
    void setCurve(const QVariantList &points);

    EngineClient *m_client = nullptr;
    QTimer m_timer;
    QElapsedTimer m_sinceRequest;
    QString m_element;
    int m_quietTicks = 0;
    bool m_inFlight = false;

    bool m_available = false;
    QVariantList m_points;
    QString m_path;
    QString m_fill;
    int m_dbTop = kMinRangeDb;
    int m_dbBottom = -kMinRangeDb;
    QString m_gainText;
    bool m_autoGain = false;
    QString m_model;
};

} // namespace rigpa
