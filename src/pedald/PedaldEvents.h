#pragma once

#include "PedaldClient.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QNetworkReply;

namespace rigpa {

// pedald's event stream (pedal.project docs/pedal-service.md, "The pedal's identity and its event
// stream"): `GET /events`, server-sent events, held open. What the pedal says happened arrives here,
// with its cause: a footswitch and what it did, the bank, the library, a save, the undo history.
//
// It keeps itself connected, and a reconnect sends the last id it saw, so nothing is missed unless the
// service says `resync`, when a `hello` with the whole state follows. A stream that goes quiet for
// longer than the service's keep-alive is taken to be dead and opened again.
class PedaldEvents : public QObject
{
    Q_OBJECT

public:
    struct Event {
        QString id;
        QString kind;
        QJsonObject data;
    };

    static constexpr int kQuietMs = 40000;   // pedald sends a keep-alive every 15 s
    static constexpr int kFirstMs = 5000;    // and its hello at once
    static constexpr int kRetryMs = 1000;
    static constexpr int kRetryMaxMs = 5000;

    explicit PedaldEvents(PedaldClient *pedald, QObject *parent = nullptr);
    ~PedaldEvents() override;

    void start();
    void stop();
    bool connected() const { return m_connected; }

    // The complete events at the front of `buffer`, which keeps whatever is left of the next one.
    // Comments (": ka") and fields this doesn't know are skipped.
    static QList<Event> parse(QByteArray &buffer);

signals:
    void event(const QString &kind, const QJsonObject &data);
    void connectedChanged();

private:
    void open();
    void onData();
    void onEnded();
    void setConnected(bool connected);

    PedaldClient *m_pedald;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_buffer;
    QString m_lastId;
    QTimer m_retry;
    QTimer m_quiet;
    int m_wait = kRetryMs;
    bool m_running = false;
    bool m_connected = false;
};

} // namespace rigpa
