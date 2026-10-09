#pragma once

#include "Framing.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

#include <functional>

namespace rigpa {

// One reply from namchaind: {"ok":true,...} or {"ok":false,"error":{"code","message"}}.
struct EngineReply {
    bool ok = false;
    QJsonObject body;
    QString code;     // set when !ok
    QString message;  // set when !ok
};

// A connection to namchaind over TCP (namchain docs/daemon.md): u32-LE framed JSON, `engine.hello`
// with the token first, then requests matched to replies by their string id. Nothing is ever
// pushed: everything here is an answer to something asked.
//
// It keeps itself connected: a failure or a drop is retried after 1, 2, 5 and 10 s, then every
// 30 s (ljoss's schedule), except a refusal that retrying can't fix (`unauthorized`,
// `unsupported_protocol`), which stops until `connectTo` is called again.
class EngineClient : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Connecting, Authenticating, Ready, Waiting, Refused };
    Q_ENUM(State)

    static constexpr int kProtocol = 2;
    using Callback = std::function<void(const EngineReply &)>;

    explicit EngineClient(QObject *parent = nullptr);
    ~EngineClient() override;

    void connectTo(const QString &host, quint16 port, const QString &token);
    void disconnectFromEngine();

    // Sends one request; `done` gets its reply, or a failure (`not_connected`, `timeout`,
    // `disconnected`) if there won't be one. Fields in `params` sit beside "id" and "method".
    void request(const QString &method, const QJsonObject &params = {}, Callback done = {});

    State state() const { return m_state; }
    bool isReady() const { return m_state == State::Ready; }
    QStringList features() const { return m_features; }
    bool hasFeature(const QString &feature) const { return m_features.contains(feature); }
    QString version() const { return m_version; }
    // Why the last attempt failed, for people.
    QString lastError() const { return m_lastError; }
    // When the next attempt will be made, while Waiting.
    int retryInMs() const;

    // For tests: the waits between attempts, and the per-request timeout.
    void setBackoff(const QList<int> &ms) { m_backoff = ms; }
    void setReplyTimeout(int ms) { m_replyTimeoutMs = ms; }

signals:
    void stateChanged(rigpa::EngineClient::State state);
    // Each time a connection is authenticated and usable.
    void ready();

private:
    struct Pending {
        Callback done;
        qint64 sentAt;
    };

    void setState(State state);
    void abortSocket();
    void open();
    void onConnected();
    void onReadyRead();
    void onSocketError();
    void handle(const QJsonObject &object);
    void fail(const QString &why, bool retry);
    void failAllPending(const QString &code, const QString &message);
    void checkTimeouts();
    void send(const QString &id, const QString &method, const QJsonObject &params);

    QTcpSocket m_socket;
    framing::Reader m_reader;
    QTimer m_retryTimer;
    QTimer m_timeoutTimer;
    QElapsedTimer m_clock;
    QString m_host;
    quint16 m_port = 0;
    QString m_token;
    State m_state = State::Idle;
    QHash<QString, Pending> m_pending;
    quint64 m_nextId = 1;
    QString m_helloId;
    QStringList m_features;
    QString m_version;
    QString m_lastError;
    QList<int> m_backoff{1000, 2000, 5000, 10000, 30000};
    int m_attempt = 0;
    int m_replyTimeoutMs = 10000;
};

} // namespace rigpa
