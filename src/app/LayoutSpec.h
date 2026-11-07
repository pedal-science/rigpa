#pragma once

#include "rig/RigEditorModel.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// Which set of views rigpa shows, and every size that differs between them, in one place.
//
//   wide     the 8.8" bar, 1920×480: | menu | header / content / footer | info |, the views the
//            spec draws (docs/spec.md, Layout). Every number here is what it was before there
//            were two layouts, so the bar does not move by a pixel.
//   compact  the box's 4.3" panel, 800×480 landscape: a top bar and a bottom bar around one view at
//            a time, with the play view as home (docs/spec.md, "The compact layout").
//
// Both panels are about 220 pixels to the inch, so a font size reads the same on either: compact
// keeps the bar's type and spends its smaller area on showing less at once, not on smaller text.
// The models stay shared (rigpa_core); only QML and the few sizes C++ lays out with differ.
class LayoutSpec : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the layout")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool compact READ compact CONSTANT)
    Q_PROPERTY(int screenWidth READ screenWidth CONSTANT)
    Q_PROPERTY(int screenHeight READ screenHeight CONSTANT)
    Q_PROPERTY(int menuWidth READ menuWidth CONSTANT)
    Q_PROPERTY(int headerHeight READ headerHeight CONSTANT)
    Q_PROPERTY(int footerHeight READ footerHeight CONSTANT)
    Q_PROPERTY(int infoWidth READ infoWidth CONSTANT)
    Q_PROPERTY(int contentWidth READ contentWidth CONSTANT)
    Q_PROPERTY(int contentHeight READ contentHeight CONSTANT)
    Q_PROPERTY(int playTitleFont READ playTitleFont CONSTANT)
    Q_PROPERTY(int tunerBarWidth READ tunerBarWidth CONSTANT)
    Q_PROPERTY(int tunerNoteWidth READ tunerNoteWidth CONSTANT)
    Q_PROPERTY(int tunerNoteFont READ tunerNoteFont CONSTANT)
    Q_PROPERTY(int tunerScaleHeight READ tunerScaleHeight CONSTANT)
    Q_PROPERTY(int flowEqCurveWidth READ flowEqCurveWidth CONSTANT)
    Q_PROPERTY(int flowEqCurveHeight READ flowEqCurveHeight CONSTANT)
    Q_PROPERTY(int keyboardWidth READ keyboardWidth CONSTANT)

public:
    enum class Kind { Wide, Compact };

    explicit LayoutSpec(Kind kind = Kind::Wide, QObject *parent = nullptr);
    // "wide" or "compact" (Config::isLayout has already refused anything else).
    static Kind kindNamed(const QString &name);

    Kind kind() const { return m_kind; }
    QString name() const;
    bool compact() const { return m_kind == Kind::Compact; }

    int screenWidth() const { return compact() ? 800 : 1920; }
    int screenHeight() const { return 480; }
    // The bar's columns; compact has neither a menu nor an info column.
    int menuWidth() const { return compact() ? 0 : 96; }
    int infoWidth() const { return compact() ? 0 : 320; }
    // The header and footer: compact's top and bottom bars are a little shorter.
    int headerHeight() const { return compact() ? 52 : 64; }
    int footerHeight() const { return compact() ? 40 : 48; }
    // What a main view gets: 1504×368 on the bar, 800×388 on the panel.
    int contentWidth() const { return screenWidth() - menuWidth() - infoWidth(); }
    int contentHeight() const { return screenHeight() - headerHeight() - footerHeight(); }

    // The play view's rig name: big enough to read from standing over a floor pedal.
    int playTitleFont() const { return 64; }

    // The tuner's cents scale (TunerModel::setBarWidth), the note beside it, and its height.
    int tunerBarWidth() const { return compact() ? 560 : 720; }
    int tunerNoteWidth() const { return compact() ? 170 : 280; }
    int tunerNoteFont() const { return compact() ? 96 : 132; }
    int tunerScaleHeight() const { return compact() ? 150 : 200; }

    // flowEQ's response curve (FlowEqModel's path is drawn at this size).
    int flowEqCurveWidth() const { return compact() ? 460 : 560; }
    int flowEqCurveHeight() const { return compact() ? 188 : 236; }

    // Qt Virtual Keyboard's width; its height follows (about 0.31 of it), so a dialog's name row
    // has to sit in the top ~230 px of the screen on the panel.
    int keyboardWidth() const { return compact() ? 800 : 880; }

    // The rig editor's tiles and spacing (RigEditorModel::setStyle).
    RigEditorModel::Style editorStyle() const;

private:
    Kind m_kind;
};

} // namespace rigpa
