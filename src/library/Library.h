#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace rigpa {

// One file in the library, as rigpa reads it (read-only, directly: namchaind doesn't read NAM
// metadata, and the library is shared with it).
struct LibraryEntry {
    enum Kind { Model, Ir };
    Kind kind = Model;
    QString id;        // the path under its root without the extension: what a rig names
    QString path;      // absolute
    QString group;     // the folders between the root and the file, '/'-joined ("" at the top)
    QString name;      // metadata.name, else the file's stem
    QString make;      // a brand, or "Unknown"
    QString model;     // metadata.gear_model
    QString gearType;
    QString architecture;
    double sampleRate = 0;
    qint64 frames = -1;
    int channels = 0;
    qint64 size = 0;
    QDateTime modified;
    bool isIrNam = false; // a .nam whose architecture is Linear: a convolution, under irs/
};

// The library: both roots, scanned on a worker thread (each .nam read in a 16 KiB + 64 KiB
// window, each .wav's header), cached by size and modification time so a rescan only reads
// what changed. Rows are models or IRs (`kind`), ordered by group then name, so the list's
// sections are its folders.
class Library : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app owns the library")
    Q_PROPERTY(int kind READ kind WRITE setKind NOTIFY kindChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int modelCount READ modelCount NOTIFY scanned)
    Q_PROPERTY(int irCount READ irCount NOTIFY scanned)
    Q_PROPERTY(QString problem READ problem NOTIFY scanned)

public:
    enum Role {
        IdRole = Qt::UserRole + 1, NameRole, GroupRole, MakeRole, ModelRole, GearTypeRole,
        ArchitectureRole, SampleRateRole, FramesRole, ChannelsRole, SizeRole, DetailRole,
    };

    explicit Library(QObject *parent = nullptr);

    void setRoots(const QString &models, const QString &irs);
    QString modelsRoot() const { return m_modelsRoot; }
    QString irsRoot() const { return m_irsRoot; }

    // Rescans in the background; only files whose size or time changed are read again.
    Q_INVOKABLE void rescan();

    int kind() const { return m_kind; }
    void setKind(int kind);
    bool scanning() const { return m_watcher.isRunning(); }
    int modelCount() const { return int(m_models.size()); }
    int irCount() const { return int(m_irs.size()); }
    QString problem() const { return m_problem; }

    // An entry by kind and id, for showing what a rig names.
    const LibraryEntry *find(LibraryEntry::Kind kind, const QString &id) const;
    const QList<LibraryEntry> &entries(LibraryEntry::Kind kind) const
    {
        return kind == LibraryEntry::Model ? m_models : m_irs;
    }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // "make · model · 48 kHz · 8,192 frames · 69 KB", leaving out what isn't known.
    static QString detail(const LibraryEntry &e);

    // Synchronous, for tests and the worker: every entry under a root.
    struct ScanResult {
        QList<LibraryEntry> models;
        QList<LibraryEntry> irs;
        QString problem;
    };
    using Cache = QHash<QString, LibraryEntry>; // path -> entry (size and modified say if it's current)
    static ScanResult scan(const QString &modelsRoot, const QString &irsRoot, const Cache &cache);
    static LibraryEntry read(LibraryEntry::Kind kind, const QString &root, const QString &path);

signals:
    void kindChanged();
    void scanningChanged();
    void scanned();

private:
    const QList<LibraryEntry> &current() const { return entries(LibraryEntry::Kind(m_kind)); }

    QString m_modelsRoot;
    QString m_irsRoot;
    int m_kind = LibraryEntry::Model;
    QList<LibraryEntry> m_models;
    QList<LibraryEntry> m_irs;
    QString m_problem;
    Cache m_cache;
    QFutureWatcher<ScanResult> m_watcher;
    bool m_rescanAgain = false;
};

} // namespace rigpa
