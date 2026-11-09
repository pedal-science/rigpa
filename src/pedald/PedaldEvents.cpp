#include "PedaldEvents.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace rigpa {

PedaldEvents::PedaldEvents(PedaldClient *pedald, QObject *parent) : QObject(parent), m_pedald(pedald)
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &PedaldEvents::open);
    m_quiet.setSingleShot(true);
    m_quiet.setInterval(kQuietMs);
    connect(&m_quiet, &QTimer::timeout, this, [this] {
        if (m_reply)
            m_reply->abort(); // onEnded opens it again
    });
}

PedaldEvents::~PedaldEvents()
{
    stop();
}

void PedaldEvents::start()
{
    m_running = true;
    open();
}

void PedaldEvents::stop()
{
    m_running = false;
    m_retry.stop();
    m_quiet.stop();
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    setConnected(false);
}

void PedaldEvents::open()
{
    if (!m_running || m_reply)
        return;
    if (!m_pedald->configured()) {
        m_retry.start(kRetryMaxMs);
        return;
    }
    QNetworkRequest request(m_pedald->url(QStringLiteral("/events")));
    request.setRawHeader("Authorization", "Bearer " + m_pedald->token().toUtf8());
    request.setRawHeader("Accept", "text/event-stream");
    if (!m_lastId.isEmpty())
        request.setRawHeader("Last-Event-ID", m_lastId.toUtf8());
    request.setTransferTimeout(0);
    m_buffer.clear();
    m_reply = m_pedald->network()->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &PedaldEvents::onData);
    connect(m_reply, &QNetworkReply::finished, this, &PedaldEvents::onEnded);
    // A request that hasn't been answered in a few seconds won't be: a service still starting can
    // take the connection and never reply to it.
    m_quiet.start(kFirstMs);
}

void PedaldEvents::onData()
{
    if (!m_reply)
        return;
    m_quiet.start(kQuietMs);
    if (m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
        m_reply->readAll(); // an error's body, read when it has ended
        return;
    }
    m_wait = kRetryMs;
    setConnected(true);
    m_buffer += m_reply->readAll();
    for (const Event &e : parse(m_buffer)) {
        if (!e.id.isEmpty())
            m_lastId = e.id;
        emit event(e.kind, e.data);
    }
}

void PedaldEvents::onEnded()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_quiet.stop();
    if (reply)
        reply->deleteLater();
    setConnected(false);
    if (!m_running)
        return;
    m_retry.start(m_wait);
    m_wait = std::min(m_wait * 2, kRetryMaxMs);
}

void PedaldEvents::setConnected(bool connected)
{
    if (connected == m_connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}

QList<PedaldEvents::Event> PedaldEvents::parse(QByteArray &buffer)
{
    QList<Event> out;
    buffer.replace("\r\n", "\n");
    qsizetype end;
    while ((end = buffer.indexOf("\n\n")) >= 0) {
        const QByteArray block = buffer.left(end);
        buffer.remove(0, end + 2);
        Event e;
        QByteArray data;
        for (const QByteArray &line : block.split('\n')) {
            if (line.isEmpty() || line.startsWith(':'))
                continue;
            const qsizetype colon = line.indexOf(':');
            const QByteArray field = colon < 0 ? line : line.left(colon);
            QByteArray value = colon < 0 ? QByteArray() : line.mid(colon + 1);
            if (value.startsWith(' '))
                value.remove(0, 1);
            if (field == "id")
                e.id = QString::fromUtf8(value);
            else if (field == "event")
                e.kind = QString::fromUtf8(value);
            else if (field == "data")
                data += (data.isEmpty() ? QByteArray() : QByteArray("\n")) + value;
        }
        if (e.kind.isEmpty() && data.isEmpty())
            continue;
        if (e.kind.isEmpty())
            e.kind = QStringLiteral("message");
        e.data = QJsonDocument::fromJson(data).object();
        out.append(e);
    }
    return out;
}

} // namespace rigpa
