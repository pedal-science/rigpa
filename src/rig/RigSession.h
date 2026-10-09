#pragma once

#include "RigDocument.h"
#include "RigList.h"
#include "engine/EngineSession.h"

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

public:
    RigSession(EngineSession *engine, RigList *rigs, QObject *parent = nullptr);

    const RigDocument &document() const { return m_document; }
    bool hasDocument() const { return m_hasDocument; }
    // Node ids whose model or IR the engine couldn't find (from rig.get's missing_assets).
    const QSet<QString> &missingNodes() const { return m_missing; }
    QString message() const { return m_message; }

    Q_INVOKABLE void loadRig(int index);
    Q_INVOKABLE void loadRigNamed(const QString &name);

    // For tests: which library rig an engine status names.
    static QString runningName(const QString &rigPath, const QString &rigName, const QStringList &names);

signals:
    void documentChanged();
    void messageChanged();

private:
    void fetchDocument();
    void updateRunning();
    void maybeLoadFirst();
    void say(const QString &message);

    EngineSession *m_engine;
    RigList *m_rigs;
    RigDocument m_document;
    bool m_hasDocument = false;
    QSet<QString> m_missing;
    QString m_message;
    bool m_fetching = false;
    bool m_fetchAgain = false;
    bool m_startupDone = false;
    qint64 m_seenParams = -1;
};

} // namespace rigpa
