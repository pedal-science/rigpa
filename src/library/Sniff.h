#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace rigpa {

// What a .nam file says about itself, read without loading the model (a port of ljoss's NAMSniff,
// which matches namchain's registry): the keys found in a bounded window at the front and the
// back of the file. A model's weights are megabytes of numbers; nothing here reads them.
struct NamInfo {
    QString architecture;
    double sampleRate = 0;  // 0: the file didn't say
    QString name;           // metadata.name
    QString make;           // metadata.gear_make
    QString model;          // metadata.gear_model
    QString gearType;       // metadata.gear_type
    QString toneType;       // metadata.tone_type
    QString modeledBy;      // metadata.modeled_by
};

struct WavInfo {
    double sampleRate = 0;
    int channels = 0;
    qint64 frames = -1;  // -1: not found in the header window
    int bits = 0;
};

namespace sniff {

inline constexpr qsizetype kNamHead = 16 * 1024;
// Bigger than ljoss's 1 KiB: newer files (0.7) put `metadata` after the weights, at the end.
inline constexpr qsizetype kNamTail = 64 * 1024;
inline constexpr qsizetype kWavHeader = 64 * 1024;
// The architecture of a .nam that is an impulse response (the core's Linear model).
inline constexpr const char *kIrArchitecture = "Linear";

// From the front and back windows of a file (the whole file when it is smaller than both).
NamInfo nam(const QByteArray &head, const QByteArray &tail);
NamInfo namFile(const QString &path);

std::optional<WavInfo> wav(const QByteArray &header);
std::optional<WavInfo> wavFile(const QString &path);

// The text of the JSON object that is the value of `"key"` in `text`, by brace matching (aware of
// strings and escapes), or empty when the window doesn't hold all of it.
QByteArray objectAfterKey(const QByteArray &text, const QByteArray &key);

} // namespace sniff
} // namespace rigpa
