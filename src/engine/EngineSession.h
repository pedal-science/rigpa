#pragma once

#include "EngineClient.h"
#include "Schema.h"

#include <QMap>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// The engine as the screen sees it. namchaind pushes nothing, so this polls: `engine.status` twice
// a second and, while the meters are on, `engine.levels` ten times a second (one request of each
// kind in flight at most). Everything QML shows about the engine comes from here.
class EngineSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the engine session")
    Q_PROPERTY(Connection connection READ connection NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionText READ connectionText NOTIFY connectionChanged)
    Q_PROPERTY(QString engineVersion READ engineVersion NOTIFY connectionChanged)
    Q_PROPERTY(QString state READ engineState NOTIFY statusChanged)
    Q_PROPERTY(QString health READ health NOTIFY statusChanged)
    Q_PROPERTY(QString issue READ issue NOTIFY statusChanged)
    Q_PROPERTY(double dspPercent READ dspPercent NOTIFY statusChanged)
    Q_PROPERTY(qint64 xruns READ xruns NOTIFY statusChanged)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY statusChanged)
    Q_PROPERTY(int periodSize READ periodSize NOTIFY statusChanged)
    Q_PROPERTY(QString rigName READ rigName NOTIFY rigChanged)
    Q_PROPERTY(QString rigPath READ rigPath NOTIFY rigChanged)
    Q_PROPERTY(bool hasRig READ hasRig NOTIFY rigChanged)
    Q_PROPERTY(QVariantList inputPeaks READ inputPeaks NOTIFY levelsChanged)
    Q_PROPERTY(QVariantList outputPeaks READ outputPeaks NOTIFY levelsChanged)
    Q_PROPERTY(bool metersEnabled READ metersEnabled WRITE setMetersEnabled NOTIFY metersEnabledChanged)

public:
    enum Connection { NotConfigured, Connecting, Connected, Retrying, Refused };
    Q_ENUM(Connection)

    static constexpr int kStatusIntervalMs = 500;
    static constexpr int kLevelsIntervalMs = 100;
    // A meter shows nothing below this.
    static constexpr double kFloorDb = -60.0;

    explicit EngineSession(QObject *parent = nullptr);

    // Starts connecting. An empty token means the configuration couldn't provide one, which is
    // reported rather than tried.
    void start(const QString &host, quint16 port, const QString &token, const QString &problem = {});

    EngineClient *client() { return &m_client; }
    // The node catalogue, read once per connection (empty until it arrives).
    const Schema &schema() const { return m_schema; }

    Connection connection() const;
    QString connectionText() const;
    QString engineVersion() const { return m_client.version(); }
    QString engineState() const { return m_engineState; }
    QString health() const { return m_health; }
    QString issue() const { return m_issue; }
    double dspPercent() const { return m_dspPercent; }
    qint64 xruns() const { return m_xruns; }
    int sampleRate() const { return m_sampleRate; }
    int periodSize() const { return m_periodSize; }
    QString rigName() const { return m_rigName; }
    QString rigPath() const { return m_rigPath; }
    bool hasRig() const { return !m_rigPath.isEmpty() || m_generation > 0; }
    qint64 generation() const { return m_generation; }
    // The document's own `name` ("" when it has none), unlike rigName, which falls back to the file.
    QString documentName() const { return m_documentName; }
    qint64 paramsApplied() const { return m_paramsApplied; }
    // A status has arrived on this connection.
    bool hasStatus() const { return m_haveHistogram; }
    QVariantList inputPeaks() const { return m_inputPeaks; }
    QVariantList outputPeaks() const { return m_outputPeaks; }
    bool metersEnabled() const { return m_metersEnabled; }
    void setMetersEnabled(bool on);

    // Exposed for tests: digest one engine.status / engine.levels reply body.
    void applyStatus(const QJsonObject &status);
    void applyLevels(const QJsonObject &levels);

    // The worst block load (percent of the deadline) in the blocks rendered between two readings
    // of `render.load_histogram` (cumulative [percent, count] pairs); -1 when nothing new rendered.
    static double recentWorstPercent(const QMap<int, qint64> &before, const QMap<int, qint64> &after);

signals:
    void connectionChanged();
    void statusChanged();
    void rigChanged();
    void levelsChanged();
    void metersEnabledChanged();
    void schemaChanged();

private:
    void pollStatus();
    void pollLevels();
    void fetchSchema();
    void clearReadings();

    EngineClient m_client;
    Schema m_schema;
    QTimer m_statusTimer;
    QTimer m_levelsTimer;
    bool m_statusInFlight = false;
    bool m_levelsInFlight = false;
    bool m_metersEnabled = true;
    QString m_problem;

    QString m_engineState;
    QString m_health;
    QString m_issue;
    double m_dspPercent = -1;
    double m_dspPrevious = -1;
    qint64 m_xruns = 0;
    int m_sampleRate = 0;
    int m_periodSize = 0;
    QString m_rigName;
    QString m_rigPath;
    qint64 m_generation = 0;
    QString m_documentName;
    qint64 m_paramsApplied = 0;
    QMap<int, qint64> m_histogram;
    bool m_haveHistogram = false;
    QVariantList m_inputPeaks;
    QVariantList m_outputPeaks;
};

} // namespace rigpa
