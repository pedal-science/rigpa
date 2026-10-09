#include "Framing.h"

#include <QJsonDocument>
#include <QtEndian>

namespace rigpa::framing {

QByteArray encode(const QJsonObject &object, QString *error)
{
    const QByteArray body = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (body.size() > kMaxRequestBytes) {
        if (error)
            *error = QStringLiteral("request of %1 bytes is over the %2-byte limit")
                         .arg(body.size()).arg(kMaxRequestBytes);
        return {};
    }
    QByteArray frame(4, Qt::Uninitialized);
    qToLittleEndian<quint32>(quint32(body.size()), frame.data());
    frame.append(body);
    return frame;
}

void Reader::append(const QByteArray &bytes)
{
    if (broken())
        return;
    m_buffer.append(bytes);
    for (;;) {
        if (m_buffer.size() < 4)
            return;
        const quint32 length = qFromLittleEndian<quint32>(m_buffer.constData());
        // Checked before waiting for the body, so a corrupt length can't make us buffer forever.
        if (length > quint32(kMaxReplyBytes)) {
            m_error = QStringLiteral("reply of %1 bytes is over the %2-byte limit").arg(length).arg(kMaxReplyBytes);
            m_buffer.clear();
            return;
        }
        if (m_buffer.size() < 4 + qsizetype(length))
            return;
        QJsonParseError parse;
        const QJsonDocument doc = QJsonDocument::fromJson(m_buffer.mid(4, length), &parse);
        m_buffer.remove(0, 4 + length);
        if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
            m_error = QStringLiteral("reply is not a JSON object: %1").arg(parse.errorString());
            m_buffer.clear();
            return;
        }
        m_ready.append(doc.object());
    }
}

QList<QJsonObject> Reader::take()
{
    return std::exchange(m_ready, {});
}

void Reader::reset()
{
    m_buffer.clear();
    m_ready.clear();
    m_error.clear();
}

} // namespace rigpa::framing
