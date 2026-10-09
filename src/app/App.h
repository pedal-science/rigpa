#pragma once

#include "Config.h"
#include "engine/EngineSession.h"
#include "library/Library.h"
#include "library/PickerModel.h"
#include "rig/NodeEditorModel.h"
#include "rig/PaletteModel.h"
#include "rig/RigEditorModel.h"
#include "rig/RigList.h"
#include "rig/RigSession.h"
#include "Navigator.h"
#include "Settings.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

namespace rigpa {

// What QML sees of the app: one singleton, `App`, made in main() before the engine loads.
class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(rigpa::Config *config READ config CONSTANT)
    Q_PROPERTY(rigpa::Settings *settings READ settings CONSTANT)
    Q_PROPERTY(rigpa::Navigator *nav READ nav CONSTANT)
    Q_PROPERTY(rigpa::EngineSession *engine READ engine CONSTANT)
    Q_PROPERTY(rigpa::Library *library READ library CONSTANT)
    Q_PROPERTY(rigpa::RigList *rigs READ rigs CONSTANT)
    Q_PROPERTY(rigpa::RigSession *rig READ rig CONSTANT)
    Q_PROPERTY(rigpa::RigEditorModel *editor READ editor CONSTANT)
    Q_PROPERTY(rigpa::NodeEditorModel *nodeEditor READ nodeEditor CONSTANT)
    Q_PROPERTY(rigpa::PickerModel *picker READ picker CONSTANT)
    Q_PROPERTY(rigpa::PaletteModel *palette READ palette CONSTANT)

public:
    App(Config *config, Settings *settings, Navigator *nav, EngineSession *engine,
        Library *library, RigList *rigs, RigSession *rig, RigEditorModel *editor,
        NodeEditorModel *nodeEditor, PickerModel *picker, PaletteModel *palette, QObject *parent = nullptr);
    ~App() override;

    static App *create(QQmlEngine *, QJSEngine *);

    Config *config() const { return m_config; }
    Settings *settings() const { return m_settings; }
    Navigator *nav() const { return m_nav; }
    EngineSession *engine() const { return m_engine; }
    Library *library() const { return m_library; }
    RigList *rigs() const { return m_rigs; }
    RigSession *rig() const { return m_rig; }
    RigEditorModel *editor() const { return m_editor; }
    NodeEditorModel *nodeEditor() const { return m_nodeEditor; }
    PickerModel *picker() const { return m_picker; }
    PaletteModel *palette() const { return m_palette; }

    // The header's +: the palette, over the rig view. What is chosen goes in after the selected
    // block (in its lane, if it is in one), or at the end of the first chain.
    Q_INVOKABLE void openPalette();
    void add(int paletteRow);

    // Opens the node editor on the rig editor's selectable `select` (apply, or a tap on the
    // selected tile), over the rig view.
    Q_INVOKABLE void openNodeEditor(int select);
    // Closes the node editor and anything open over it.
    Q_INVOKABLE void closeNodeEditor();

private:
    Config *m_config;
    Settings *m_settings;
    Navigator *m_nav;
    EngineSession *m_engine;
    Library *m_library;
    RigList *m_rigs;
    RigSession *m_rig;
    RigEditorModel *m_editor;
    NodeEditorModel *m_nodeEditor;
    PickerModel *m_picker;
    PaletteModel *m_palette;
};

} // namespace rigpa
