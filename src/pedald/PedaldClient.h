#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

#include <functional>

namespace rigpa {

struct PedaldReply {
    bool ok = false;
    int status = 0;          // HTTP status, 0 when the service couldn't be reached
    QJsonDocument body;
    QString error;           // for people
};

// pedald's HTTP API (pedal.project docs/pedal-service.md): JSON, `Authorization: Bearer <token>`
// (the engine's token), on 127.0.0.1:47820 by default. rigpa uses its rig library half.
class PedaldClient : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const PedaldReply &)>;

    explicit PedaldClient(QObject *parent = nullptr);

    void setEndpoint(const QString &host, quint16 port, const QString &token);
    bool configured() const { return !m_token.isEmpty(); }

    void get(const QString &path, Callback done);
    void put(const QString &path, const QByteArray &body, Callback done);
    void post(const QString &path, const QByteArray &body, Callback done);
    void remove(const QString &path, Callback done);

    // "/library/rigs/<name>" with the name percent-encoded.
    static QString rigPath(const QString &name, const QString &suffix = {});

private:
    void send(const QByteArray &verb, const QString &path, const QByteArray &body, Callback done);

    QNetworkAccessManager m_network;
    QUrl m_base;
    QString m_token;
};

} // namespace rigpa
