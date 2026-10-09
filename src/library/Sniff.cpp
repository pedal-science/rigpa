#include "Sniff.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtEndian>

namespace rigpa::sniff {

namespace {

QString stringField(const QByteArray &text, const char *key)
{
    const QRegularExpression re(QStringLiteral("\"%1\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"").arg(QLatin1String(key)));
    const auto m = re.match(QString::fromUtf8(text));
    if (!m.hasMatch())
        return {};
    // Let the JSON parser undo the escapes.
    const QJsonDocument doc = QJsonDocument::fromJson("[\"" + m.captured(1).toUtf8() + "\"]");
    return doc.isArray() ? doc.array().at(0).toString() : m.captured(1);
}

double numberField(const QByteArray &text, const char *key)
{
    const QRegularExpression re(QStringLiteral("\"%1\"\\s*:\\s*(-?[0-9][0-9.eE+-]*)").arg(QLatin1String(key)));
    const auto m = re.match(QString::fromUtf8(text));
    return m.hasMatch() ? m.captured(1).toDouble() : 0;
}

QString text(const QJsonObject &o, const char *key)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isString() ? v.toString().trimmed() : QString();
}

quint32 u32(const QByteArray &b, qsizetype at) { return qFromLittleEndian<quint32>(b.constData() + at); }
quint16 u16(const QByteArray &b, qsizetype at) { return qFromLittleEndian<quint16>(b.constData() + at); }

} // namespace

QByteArray objectAfterKey(const QByteArray &text, const QByteArray &key)
{
    const QByteArray quoted = '"' + key + '"';
    qsizetype at = text.indexOf(quoted);
    while (at >= 0) {
        qsizetype i = at + quoted.size();
        while (i < text.size() && (text[i] == ' ' || text[i] == '\n' || text[i] == '\r' || text[i] == '\t'))
            ++i;
        if (i < text.size() && text[i] == ':') {
            ++i;
            while (i < text.size() && (text[i] == ' ' || text[i] == '\n' || text[i] == '\r' || text[i] == '\t'))
                ++i;
            if (i < text.size() && text[i] == '{') {
                int depth = 0;
                bool inString = false;
                for (qsizetype j = i; j < text.size(); ++j) {
                    const char c = text[j];
                    if (inString) {
                        if (c == '\\')
                            ++j;
                        else if (c == '"')
                            inString = false;
                    } else if (c == '"') {
                        inString = true;
                    } else if (c == '{') {
                        ++depth;
                    } else if (c == '}' && --depth == 0) {
                        return text.mid(i, j - i + 1);
                    }
                }
                return {}; // the window ends inside it
            }
        }
        at = text.indexOf(quoted, at + 1);
    }
    return {};
}

NamInfo nam(const QByteArray &head, const QByteArray &tail)
{
    NamInfo info;
    for (const QByteArray *window : {&head, &tail}) {
        if (info.architecture.isEmpty())
            info.architecture = stringField(*window, "architecture");
        if (info.sampleRate <= 0)
            info.sampleRate = numberField(*window, "sample_rate");
        if (info.name.isEmpty() && info.make.isEmpty() && info.model.isEmpty()) {
            const QJsonObject metadata = QJsonDocument::fromJson(objectAfterKey(*window, "metadata")).object();
            info.name = text(metadata, "name");
            info.make = text(metadata, "gear_make");
            info.model = text(metadata, "gear_model");
            info.gearType = text(metadata, "gear_type");
            info.toneType = text(metadata, "tone_type");
            info.modeledBy = text(metadata, "modeled_by");
        }
    }
    if (info.sampleRate < 0)
        info.sampleRate = 0;
    return info;
}

NamInfo namFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const qint64 size = file.size();
    if (size <= kNamHead + kNamTail)
        return nam(file.readAll(), {});
    const QByteArray head = file.read(kNamHead);
    file.seek(size - kNamTail);
    return nam(head, file.read(kNamTail));
}

std::optional<WavInfo> wav(const QByteArray &h)
{
    if (h.size() < 12 || h.mid(0, 4) != "RIFF" || h.mid(8, 4) != "WAVE")
        return std::nullopt;
    WavInfo info;
    bool haveFormat = false;
    qint64 dataBytes = -1;
    qsizetype at = 12;
    while (at + 8 <= h.size()) {
        const QByteArray id = h.mid(at, 4);
        const quint32 size = u32(h, at + 4);
        const qsizetype payload = at + 8;
        if (id == "fmt " && payload + 16 <= h.size()) {
            info.channels = u16(h, payload + 2);
            info.sampleRate = u32(h, payload + 4);
            info.bits = u16(h, payload + 14);
            haveFormat = true;
        } else if (id == "data") {
            dataBytes = size;
        }
        if (haveFormat && dataBytes >= 0)
            break;
        at = payload + qsizetype(size) + (size % 2); // chunks are word-aligned
    }
    if (!haveFormat)
        return std::nullopt;
    const int frameBytes = (info.bits / 8) * std::max(info.channels, 1);
    if (dataBytes >= 0 && frameBytes > 0)
        info.frames = dataBytes / frameBytes;
    return info;
}

std::optional<WavInfo> wavFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return wav(file.read(kWavHeader));
}

} // namespace rigpa::sniff
