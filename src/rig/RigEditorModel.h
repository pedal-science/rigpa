#pragma once

#include "RigDocument.h"
#include "RigEdits.h"
#include "layout/Flow.h"

#include <QAbstractListModel>
#include <QRectF>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

class Library;
class RigSession;

// The rig editor's picture of the running rig: every tile, section frame, chain label and wire,
// placed in pixels. Each chain is laid out like wrapped text (FlowArrangement, ljoss's rules);
// a parallel section is one flow item with its lanes stacked inside it, and lanes never wrap.
//
// Blocks and sections are "selectable", numbered depth first (a section before its lanes): that
// numbering is the rig editor's FocusRing, so the rotary walks the rig in signal order.
class RigEditorModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the editor model")
    Q_PROPERTY(qreal availableWidth READ availableWidth WRITE setAvailableWidth NOTIFY availableWidthChanged)
    Q_PROPERTY(qreal contentWidth READ contentWidth NOTIFY laidOut)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY laidOut)
    Q_PROPERTY(int selectableCount READ selectableCount NOTIFY laidOut)
    Q_PROPERTY(QVariantList wires READ wires NOTIFY laidOut)
    Q_PROPERTY(bool dragging READ dragging NOTIFY draggingChanged)
    Q_PROPERTY(QString liftedElement READ liftedElement NOTIFY draggingChanged)
    Q_PROPERTY(QPointF dragPoint READ dragPoint NOTIFY dragPointChanged)

public:
    struct Style {
        qreal tileWidth = 188, tileHeight = 96, ioWidth = 84;
        qreal connector = 18;      // the wire before each tile, part of its flow item
        qreal wire = 3;
        qreal rowSpacing = 30;
        qreal padding = 16;
        qreal chainLabel = 30;
        qreal chainGap = 18;
        qreal laneGap = 10;
        qreal bar = 6;             // a section's split and mix bars
        qreal emptyLane = 60;
    };

    enum Role {
        KindRole = Qt::UserRole + 1, XRole, YRole, WRole, HRole, TitleRole, SubtitleRole, ColorRole,
        BypassedRole, MissingRole, SelectRole, ElementRole, ChainRole, LiftRole,
    };

    struct Item {
        QString kind;   // "io", "block", "section", "label"
        QRectF rect;
        QString title, subtitle, color;
        bool bypassed = false, missing = false;
        int select = -1;
        QString element;
        int chain = 0;
        int lift = 0;   // 1: the lifted element's slot (drawn as a gap); 2: inside it (not drawn)
    };

    // A series the lifted element can be dropped into: a chain's, or a lane's.
    struct Container {
        int chain = 0;
        QStringList path;
        QRectF rect;
        QList<flowhit::Mark> marks;
        int count = 0;   // its elements, not counting the lifted one
    };

    explicit RigEditorModel(RigSession *session, Library *library, QObject *parent = nullptr);

    qreal availableWidth() const { return m_availableWidth; }
    void setAvailableWidth(qreal width);
    qreal contentWidth() const { return m_contentWidth; }
    qreal contentHeight() const { return m_contentHeight; }
    int selectableCount() const { return m_selectable; }
    QVariantList wires() const { return m_wires; }
    const QList<Item> &items() const { return m_items; }

    // The element of a selectable index, and back: how a selection survives a re-read.
    Q_INVOKABLE QString elementAt(int select) const;
    Q_INVOKABLE int selectOf(const QString &element) const;
    Q_INVOKABLE QRectF rectOf(int select) const;
    // The tile or frame showing an element, if it is laid out.
    const Item *itemFor(const QString &element) const;

    // Drag and drop (ljoss's drag engine, simplified): lift an element, move the pointer (in the
    // layout's coordinates), drop. While it is lifted the editor shows the rig with the element
    // already moved to where it would land, so the other tiles step aside and rows re-wrap as the
    // pointer moves; the drop applies exactly that document. A drop on the trash removes it.
    Q_INVOKABLE bool lift(const QString &element);
    Q_INVOKABLE void dragTo(const QPointF &point);
    Q_INVOKABLE void drop();
    Q_INVOKABLE void dropOnTrash();
    Q_INVOKABLE void cancelDrag();
    bool dragging() const { return !m_lifted.isEmpty(); }
    QString liftedElement() const { return m_lifted; }
    QPointF dragPoint() const { return m_dragPoint; }
    edits::Place dropPlace() const { return m_target; }
    const QList<Container> &containers() const { return m_containers; }
    // The lifted element's look, for the ghost under the pointer.
    Q_INVOKABLE QVariantMap describeElement(const QString &element) const;

    // Lays out a document without a session (tests); the session's document otherwise.
    void layout(const RigDocument &doc, const QSet<QString> &missing);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    static QString typeLabel(const QString &type);

signals:
    void availableWidthChanged();
    void laidOut();
    void draggingChanged();
    void dragPointChanged();

private:
    QSizeF elementSize(const RigElement &e) const;
    QSizeF laneSize(const QList<RigElement> &lane) const;
    void placeBlock(const RigElement &e, const QRectF &tile, int chain);
    void placeSection(const RigElement &e, QPointF origin, int chain);
    void wire(const QRectF &r);
    void describe(const RigElement &e, Item &item) const;
    void relayout();
    RigDocument preview() const;
    void endDrag();
    static QString keyOf(const Item &item);

    RigSession *m_session;
    Library *m_library;
    Style m_style;
    qreal m_availableWidth = 1200;
    QList<Item> m_items;
    QVariantList m_wires;
    qreal m_contentWidth = 0, m_contentHeight = 0;
    int m_selectable = 0;
    QSet<QString> m_missing;
    QList<Container> m_containers;
    QStringList m_path;      // while placing: the lanes being placed into
    int m_liftDepth = 0;     // while placing: inside the lifted element
    QString m_lifted;
    edits::Place m_origin, m_target;
    QPointF m_dragPoint;
};

} // namespace rigpa
