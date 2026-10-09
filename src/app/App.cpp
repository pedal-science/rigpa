#include "App.h"

#include <QJSEngine>

namespace rigpa {

namespace {
App *g_instance = nullptr;
}

App::App(Config *config, Settings *settings, Navigator *nav, EngineSession *engine, Library *library,
         RigList *rigs, RigSession *rig, RigEditorModel *editor, QObject *parent)
    : QObject(parent), m_config(config), m_settings(settings), m_nav(nav), m_engine(engine),
      m_library(library), m_rigs(rigs), m_rig(rig), m_editor(editor)
{
    Q_ASSERT(!g_instance);
    g_instance = this;
}

App::~App()
{
    g_instance = nullptr;
}

App *App::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(g_instance);
    QJSEngine::setObjectOwnership(g_instance, QJSEngine::CppOwnership);
    return g_instance;
}

} // namespace rigpa
