#include "App.h"

#include <QJSEngine>

namespace rigpa {

namespace {
App *g_instance = nullptr;
}

App::App(Config *config, Settings *settings, Navigator *nav, EngineSession *engine, Library *library,
         RigList *rigs, RigSession *rig, RigEditorModel *editor, NodeEditorModel *nodeEditor,
         PickerModel *picker, QObject *parent)
    : QObject(parent), m_config(config), m_settings(settings), m_nav(nav), m_engine(engine),
      m_library(library), m_rigs(rigs), m_rig(rig), m_editor(editor), m_nodeEditor(nodeEditor),
      m_picker(picker)
{
    Q_ASSERT(!g_instance);
    g_instance = this;

    connect(m_nav, &Navigator::nodeEditorRequested, this, &App::openNodeEditor);
    connect(m_nodeEditor, &NodeEditorModel::closeRequested, this, &App::closeNodeEditor);
    connect(m_nodeEditor, &NodeEditorModel::pickerRequested, this, [this](int index) {
        m_picker->open(m_nodeEditor->pickerKind(index), m_nodeEditor->pickerCurrent(index), index);
        m_nav->openOverlay(Navigator::Picker, m_picker->ring());
    });
    connect(m_picker, &PickerModel::chosen, this, [this](int index, const QString &id) {
        m_nodeEditor->pick(index, id);
    });
    // Whichever way an overlay goes (back, Close, a choice, a menu button), the model behind it
    // closes with it; and a model that closes itself (its block vanished) takes its overlay along.
    connect(m_nav, &Navigator::overlayChanged, this, [this] {
        if (!m_nav->hasOverlay(Navigator::Picker))
            m_picker->close();
        if (!m_nav->hasOverlay(Navigator::NodeEditor))
            m_nodeEditor->close();
    });
    connect(m_picker, &PickerModel::openChanged, this, [this] {
        if (!m_picker->isOpen())
            m_nav->closeOverlaysFrom(Navigator::Picker);
    });
    connect(m_nodeEditor, &NodeEditorModel::openChanged, this, [this] {
        if (!m_nodeEditor->isOpen())
            m_nav->closeOverlaysFrom(Navigator::NodeEditor);
    });
}

void App::openNodeEditor(int select)
{
    const QString element = m_editor->elementAt(select);
    if (element.isEmpty() || m_nav->hasOverlay(Navigator::NodeEditor))
        return;
    m_nav->showMainView(Navigator::RigView);
    m_nav->rigEditor()->setHighlight(select);
    m_nodeEditor->open(element);
    if (m_nodeEditor->isOpen())
        m_nav->openOverlay(Navigator::NodeEditor, m_nodeEditor->ring());
}

void App::closeNodeEditor()
{
    m_nav->closeOverlaysFrom(Navigator::NodeEditor);
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
