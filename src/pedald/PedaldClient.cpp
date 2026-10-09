#include "PedaldClient.h"

#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace rigpa {

PedaldClient::PedaldClient(QObject *parent) : QObject(parent) {}

void PedaldClient::setEndpoint(const QString &host, quint16 port, const QString &token)
{
    m_base = QUrl(QStringLiteral("http://%1:%2").arg(host).arg(port));
    m_token = token;
}

QString PedaldClient::rigPath(const QString &name, const QString &suffix)
{
    return QStringLiteral("/library/rigs/") + QString::fromUtf8(QUrl::toPercentEncoding(name)) + suffix;
}

void PedaldClient::get(const QString &path, Callback done) { send("GET", path, {}, std::move(done)); }
void PedaldClient::put(const QString &path, const QByteArray &body, Callback done) { send("PUT", path, body, std::move(done)); }
void PedaldClient::post(const QString &path, const QByteArray &body, Callback done) { send("POST", path, body, std::move(done)); }
void PedaldClient::remove(const QString &path, Callback done) { send("DELETE", path, {}, std::move(done)); }

void PedaldClient::send(const QByteArray &verb, const QString &path, const QByteArray &body, Callback done)
{
    if (!configured()) {
        if (done)
            done({false, 0, {}, tr("no token for the pedal service")});
        return;
    }
    QUrl url = m_base;
    url.setPath(path, QUrl::StrictMode);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    request.setTransferTimeout(5000);
    if (!body.isNull())
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_network.sendCustomRequest(request, verb, body);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        PedaldReply r;
        r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        r.body = QJsonDocument::fromJson(data);
        r.ok = reply->error() == QNetworkReply::NoError && r.status >= 200 && r.status < 300;
        if (!r.ok) {
            const QString said = r.body.object().value(QStringLiteral("error")).toString();
            r.error = !said.isEmpty() ? said : reply->errorString();
        }
        if (done)
            done(r);
    });
}

} // namespace rigpa
