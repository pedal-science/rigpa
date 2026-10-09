#pragma once

#include "Config.h"
#include "engine/EngineSession.h"
#include "library/Library.h"
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

public:
    App(Config *config, Settings *settings, Navigator *nav, EngineSession *engine,
        Library *library, RigList *rigs, RigSession *rig, RigEditorModel *editor,
        QObject *parent = nullptr);
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

private:
    Config *m_config;
    Settings *m_settings;
    Navigator *m_nav;
    EngineSession *m_engine;
    Library *m_library;
    RigList *m_rigs;
    RigSession *m_rig;
    RigEditorModel *m_editor;
};

} // namespace rigpa
