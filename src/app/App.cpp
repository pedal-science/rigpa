#include "App.h"

#include <QJSEngine>

namespace rigpa {

namespace {
App *g_instance = nullptr;
}

App::App(Config *config, Settings *settings, Navigator *nav, QObject *parent)
    : QObject(parent), m_config(config), m_settings(settings), m_nav(nav)
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
