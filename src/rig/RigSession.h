#pragma once

#include "RigDocument.h"
#include "RigList.h"
#include "engine/EditStream.h"
#include "engine/EngineSession.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// The rig the engine is running, as a document the editor can show, and switching rigs.
//
// The engine is the truth: whenever its rig, generation or applied parameters move (rigpa's
// doing, a footswitch through pedald, ljoss, namchainctl) the document is read again with
// `rig.get`. The rig list says which library rig that is: the engine's file when it loaded one
// from the library, or the document's name after a boot resumed `last.json` (pedald's rule).
//
// Startup (docs/spec.md): show what the engine runs; if it runs nothing, load the first rig in
// the list, once per connection.
class RigSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the rig session")
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    RigSession(EngineSession *engine, RigList *rigs, QObject *parent = nullptr);

    const RigDocument &document() const { return m_document; }
    bool hasDocument() const { return m_hasDocument; }
    // Node ids whose model or IR the engine couldn't find (from rig.get's missing_assets).
    const QSet<QString> &missingNodes() const { return m_missing; }
    QString message() const { return m_message; }

    Q_INVOKABLE void loadRig(int index);
    Q_INVOKABLE void loadRigNamed(const QString &name);

    // An element (a block or a section) of the document, and the chain it is in.
    const RigElement *findElement(const QString &element, QString *chainId = nullptr) const;
    // Whether exactly one macro node in a chain carries `element`: then the engine can be asked to
    // ramp that macro's parameters by element (EditStream::target).
    bool macroElementIsUnique(const QString &chainId, const QString &element) const;

    // Small edits, through the edit stream. Bypass is every bypassable node of the block at once
    // (a crossfade, no rebuild); a parameter goes to one node.
    bool isBypassed(const QString &element) const;
    Q_INVOKABLE void setBypass(const QString &element, bool bypassed);
    Q_INVOKABLE void toggleBypass(const QString &element) { setBypass(element, !isBypassed(element)); }
    void setParam(const QString &chainId, const RigNode &node, const QString &param, const QJsonValue &value);
    EditStream *edits() { return &m_edits; }
    // A structural change (a move, an added or removed node): the whole document, as one
    // `rig.apply`. Until the engine answers, `document()` is the edited one, so the editor shows
    // the change at once; a refusal is said in the footer and the engine's document read back.
    void applyDocument(const RigDocument &edited);
    bool applying() const { return m_applying > 0; }

    // Undo and redo: every edit made here (a move, an add or remove, lanes, bypass, a model, a
    // parameter) first records the document as it was; undo applies that document again. A run of
    // changes to one parameter is one step. The history is the running rig's: loading another
    // rig, or the rig changing name underneath, starts it again.
    static constexpr int kHistoryLimit = 100;
    static constexpr int kParamStepMs = 1500;
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    void clearHistory();
    // A line for the footer (empty clears it).
    void say(const QString &message);
    // Reads the document again (after an edit the engine's status doesn't show).
    void refresh() { fetchDocument(); }

    // For tests: which library rig an engine status names.
    static QString runningName(const QString &rigPath, const QString &rigName, const QStringList &names);

signals:
    void documentChanged();
    void messageChanged();
    void historyChanged();

private:
    void fetchDocument();
    void updateRunning();
    void maybeLoadFirst();
    void record();
    void applyJson(const QJsonObject &json);

    EngineSession *m_engine;
    RigList *m_rigs;
    EditStream m_edits;
    RigDocument m_document;
    bool m_hasDocument = false;
    QSet<QString> m_missing;
    QString m_message;
    int m_applying = 0;
    QList<QJsonObject> m_undo, m_redo;
    QString m_lastParamKey;
    QElapsedTimer m_lastParamAt;
    bool m_fetching = false;
    bool m_fetchAgain = false;
    bool m_startupDone = false;
    qint64 m_seenParams = -1;
};

} // namespace rigpa
