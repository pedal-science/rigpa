#include "App.h"

#include "rig/RigEdits.h"

#include <QJSEngine>

namespace rigpa {

namespace {
App *g_instance = nullptr;
}

App::App(Config *config, Settings *settings, Navigator *nav, EngineSession *engine, Library *library,
         RigList *rigs, RigSession *rig, RigEditorModel *editor, NodeEditorModel *nodeEditor,
         PickerModel *picker, PaletteModel *palette, QObject *parent)
    : QObject(parent), m_config(config), m_settings(settings), m_nav(nav), m_engine(engine),
      m_library(library), m_rigs(rigs), m_rig(rig), m_editor(editor), m_nodeEditor(nodeEditor),
      m_picker(picker), m_palette(palette)
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
        if (!m_nav->hasOverlay(Navigator::Palette))
            m_palette->close();
    });
    connect(m_palette, &PaletteModel::chosen, this, &App::add);
    connect(m_palette, &PaletteModel::openChanged, this, [this] {
        if (!m_palette->isOpen())
            m_nav->closeOverlaysFrom(Navigator::Palette);
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

void App::openPalette()
{
    if (!m_rig->hasDocument() || m_nav->overlay() != Navigator::NoOverlay)
        return;
    m_nav->showMainView(Navigator::RigView);
    m_palette->open();
    m_nav->openOverlay(Navigator::Palette, m_palette->ring());
}

void App::add(int paletteRow)
{
    QString why;
    const auto made = m_palette->make(paletteRow, &why);
    if (!made) {
        m_rig->say(why);
        return;
    }
    RigDocument doc = m_rig->document();
    edits::Place at = edits::locate(doc, m_editor->elementAt(m_nav->rigEditor()->highlight()));
    if (at.chain >= 0) {
        at.index += 1;
    } else {
        at = {-1, {}, 0};
        for (int c = 0; c < doc.chains.size() && at.chain < 0; ++c)
            if (doc.chains.at(c).editable)
                at = {c, {}, int(doc.chains.at(c).series.size())};
    }
    if (at.chain < 0 || !edits::insert(doc, *made, at)) {
        m_rig->say(tr("This rig can't be edited here"));
        return;
    }
    m_rig->applyDocument(doc);
    emit m_editor->selectRequested(made->element);
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
