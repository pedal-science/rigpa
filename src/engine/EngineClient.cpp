#include "EngineClient.h"

#include <QJsonArray>
#include <QSignalBlocker>

#include <algorithm>

namespace rigpa {

EngineClient::EngineClient(QObject *parent) : QObject(parent)
{
    m_clock.start();
    m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(&m_socket, &QTcpSocket::connected, this, &EngineClient::onConnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &EngineClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &EngineClient::onSocketError);
    connect(&m_socket, &QTcpSocket::disconnected, this, [this] {
        if (m_state == State::Ready || m_state == State::Authenticating)
            fail(tr("the engine closed the connection"), true);
    });
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &EngineClient::open);
    m_timeoutTimer.setInterval(250);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &EngineClient::checkTimeouts);
}

EngineClient::~EngineClient()
{
    // The socket outlives the request table (members go in reverse order) and says goodbye on its
    // way out: nobody is left to hear it, and no callback may run now.
    m_socket.disconnect(this);
    m_socket.abort();
}

void EngineClient::connectTo(const QString &host, quint16 port, const QString &token)
{
    disconnectFromEngine();
    m_host = host;
    m_port = port;
    m_token = token;
    m_attempt = 0;
    open();
}

void EngineClient::disconnectFromEngine()
{
    m_retryTimer.stop();
    m_timeoutTimer.stop();
    abortSocket();
    m_reader.reset();
    failAllPending(QStringLiteral("disconnected"), tr("disconnected"));
    setState(State::Idle);
}

int EngineClient::retryInMs() const
{
    return m_retryTimer.isActive() ? m_retryTimer.remainingTime() : 0;
}

void EngineClient::abortSocket()
{
    // Our own abort is not news: without this its `disconnected` would come back as a failure.
    const QSignalBlocker blocker(&m_socket);
    m_socket.abort();
}

void EngineClient::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void EngineClient::open()
{
    abortSocket();
    m_reader.reset();
    setState(State::Connecting);
    m_socket.connectToHost(m_host, m_port);
}

void EngineClient::onConnected()
{
    setState(State::Authenticating);
    m_timeoutTimer.start();
    m_helloId = QString::number(m_nextId++);
    m_pending.insert(m_helloId, {{}, m_clock.elapsed()});
    send(m_helloId, QStringLiteral("engine.hello"),
         {{QStringLiteral("protocol"), kProtocol}, {QStringLiteral("token"), m_token}});
}

void EngineClient::onReadyRead()
{
    m_reader.append(m_socket.readAll());
    for (const QJsonObject &object : m_reader.take())
        handle(object);
    if (m_reader.broken())
        fail(m_reader.error(), true);
}

void EngineClient::onSocketError()
{
    if (m_state == State::Connecting || m_state == State::Authenticating || m_state == State::Ready)
        fail(m_socket.errorString(), true);
}

void EngineClient::handle(const QJsonObject &object)
{
    const QString id = object.value(QStringLiteral("id")).toString();
    auto it = m_pending.find(id);
    if (it == m_pending.end())
        return; // nobody is waiting: nothing is pushed, so this is a stale reply
    Pending pending = it.value();
    m_pending.erase(it);

    EngineReply reply;
    reply.ok = object.value(QStringLiteral("ok")).toBool();
    reply.body = object;
    if (!reply.ok) {
        const QJsonObject error = object.value(QStringLiteral("error")).toObject();
        reply.code = error.value(QStringLiteral("code")).toString();
        reply.message = error.value(QStringLiteral("message")).toString();
    }

    if (id == m_helloId) {
        m_helloId.clear();
        if (!reply.ok) {
            const bool hopeless = reply.code == QLatin1String("unauthorized")
                                  || reply.code == QLatin1String("unsupported_protocol");
            fail(QStringLiteral("%1: %2").arg(reply.code, reply.message), !hopeless);
            return;
        }
        m_features.clear();
        for (const auto &f : object.value(QStringLiteral("features")).toArray())
            m_features.append(f.toString());
        m_version = object.value(QStringLiteral("version")).toString();
        m_attempt = 0;
        m_lastError.clear();
        setState(State::Ready);
        emit ready();
        return;
    }
    if (pending.done)
        pending.done(reply);
}

void EngineClient::request(const QString &method, const QJsonObject &params, Callback done)
{
    if (m_state != State::Ready) {
        if (done)
            done({false, {}, QStringLiteral("not_connected"), tr("not connected to the engine")});
        return;
    }
    const QString id = QString::number(m_nextId++);
    m_pending.insert(id, {std::move(done), m_clock.elapsed()});
    send(id, method, params);
}

void EngineClient::send(const QString &id, const QString &method, const QJsonObject &params)
{
    QJsonObject message = params;
    message.insert(QStringLiteral("id"), id); // the daemon wants a string
    message.insert(QStringLiteral("method"), method);
    QString error;
    const QByteArray frame = framing::encode(message, &error);
    if (frame.isEmpty()) {
        Pending pending = m_pending.take(id);
        if (pending.done)
            pending.done({false, {}, QStringLiteral("message_too_large"), error});
        return;
    }
    m_socket.write(frame);
}

void EngineClient::checkTimeouts()
{
    const qint64 now = m_clock.elapsed();
    for (const Pending &pending : std::as_const(m_pending)) {
        if (now - pending.sentAt > m_replyTimeoutMs) {
            // The daemon answers in order, so one late reply means the connection is stuck.
            fail(tr("the engine stopped answering"), true);
            return;
        }
    }
}

void EngineClient::fail(const QString &why, bool retry)
{
    m_lastError = why;
    m_timeoutTimer.stop();
    abortSocket();
    m_reader.reset();
    m_helloId.clear();
    failAllPending(QStringLiteral("disconnected"), why);
    if (!retry) {
        setState(State::Refused);
        return;
    }
    const int wait = m_backoff.isEmpty() ? 1000 : m_backoff.at(std::min<qsizetype>(m_attempt, m_backoff.size() - 1));
    ++m_attempt;
    m_retryTimer.start(wait);
    setState(State::Waiting);
}

void EngineClient::failAllPending(const QString &code, const QString &message)
{
    const auto pending = std::exchange(m_pending, {});
    for (const Pending &p : pending)
        if (p.done)
            p.done({false, {}, code, message});
}

} // namespace rigpa
