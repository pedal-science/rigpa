#pragma once

#include <QString>
#include <QStringList>

namespace rigpa::make {

// What brand a model or IR is by (a port of ljoss's MakeHeuristics): the file's own gear_make
// when it has one, reduced to a known brand; otherwise a whole-word match against folder names
// (deepest first) and the file name; otherwise "Unknown". A table, not magic.
inline const QString kUnknown = QStringLiteral("Unknown");

QString canonical(const QString &text);
QString guess(const QString &claimed, const QString &fileStem, const QStringList &directories);

} // namespace rigpa::make
