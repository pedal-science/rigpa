#include "LayoutSpec.h"

namespace rigpa {

LayoutSpec::LayoutSpec(Kind kind, QObject *parent) : QObject(parent), m_kind(kind) {}

LayoutSpec::Kind LayoutSpec::kindNamed(const QString &name)
{
    return name == QLatin1String("compact") ? Kind::Compact : Kind::Wide;
}

QString LayoutSpec::name() const
{
    return compact() ? QStringLiteral("compact") : QStringLiteral("wide");
}

RigEditorModel::Style LayoutSpec::editorStyle() const
{
    if (!compact())
        return {};
    // Two rows of tiles and their wires in the chain view's 388 px, and five or six tiles across
    // the 800: the bar's tile at about 70%, with the gaps cut to match.
    RigEditorModel::Style style;
    style.tileWidth = 132;
    style.tileHeight = 72;
    style.ioWidth = 52;
    style.connector = 12;
    style.wire = 3;
    style.rowSpacing = 22;
    style.padding = 8;
    style.chainLabel = 22;
    style.chainGap = 12;
    style.laneGap = 8;
    style.bar = 6;
    style.emptyLane = 44;
    return style;
}

} // namespace rigpa
