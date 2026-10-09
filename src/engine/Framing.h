#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace rigpa {

// namchaind's TCP framing (namchain docs/daemon.md, "The wire, byte for byte"): a u32 little-endian
// byte count, then that many bytes of one JSON object. A port of ljoss's EngineFraming.
namespace framing {

inline constexpr qsizetype kMaxRequestBytes = 1 << 20;  // the daemon's tcp_max_message_bytes
inline constexpr qsizetype kMaxReplyBytes = 16 << 20;   // engine.schema/assets can be ~1 MB each

// The frame for one object, or an empty array (and `error`) when it would be over the request cap.
QByteArray encode(const QJsonObject &object, QString *error = nullptr);

// Reassembles frames from whatever pieces a socket delivers: one read may hold half a frame or
// several. Once broken (an oversized length or a body that isn't a JSON object) it stays broken:
// the stream has lost its place and the connection must go.
class Reader
{
public:
    void append(const QByteArray &bytes);
    // Every complete object received so far, in order.
    QList<QJsonObject> take();
    bool broken() const { return !m_error.isEmpty(); }
    QString error() const { return m_error; }
    void reset();

private:
    QByteArray m_buffer;
    QList<QJsonObject> m_ready;
    QString m_error;
};

} // namespace framing
} // namespace rigpa
