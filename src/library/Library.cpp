#include "Library.h"

#include "MakeHeuristics.h"
#include "Sniff.h"

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLocale>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace rigpa {

Library::Library(QObject *parent) : QAbstractListModel(parent)
{
    connect(&m_watcher, &QFutureWatcher<ScanResult>::finished, this, [this] {
        ScanResult result = m_watcher.result();
        beginResetModel();
        m_models = std::move(result.models);
        m_irs = std::move(result.irs);
        m_problem = result.problem;
        m_cache.clear();
        for (const auto *list : {&m_models, &m_irs})
            for (const LibraryEntry &e : *list)
                m_cache.insert(e.path, e);
        endResetModel();
        emit scanningChanged();
        emit scanned();
        if (std::exchange(m_rescanAgain, false))
            rescan();
    });
}

void Library::setRoots(const QString &models, const QString &irs)
{
    m_modelsRoot = models;
    m_irsRoot = irs;
}

void Library::rescan()
{
    if (m_watcher.isRunning()) {
        m_rescanAgain = true;
        return;
    }
    m_watcher.setFuture(QtConcurrent::run(&Library::scan, m_modelsRoot, m_irsRoot, m_cache));
    emit scanningChanged();
}

void Library::setKind(int kind)
{
    kind = kind == LibraryEntry::Ir ? LibraryEntry::Ir : LibraryEntry::Model;
    if (kind == m_kind)
        return;
    beginResetModel();
    m_kind = kind;
    endResetModel();
    emit kindChanged();
}

const LibraryEntry *Library::find(LibraryEntry::Kind kind, const QString &id) const
{
    for (const LibraryEntry &e : entries(kind))
        if (e.id == id)
            return &e;
    return nullptr;
}

int Library::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(current().size());
}

QVariant Library::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= current().size())
        return {};
    const LibraryEntry &e = current().at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return e.name;
    case IdRole: return e.id;
    case GroupRole: return e.group;
    case MakeRole: return e.make;
    case ModelRole: return e.model;
    case GearTypeRole: return e.gearType;
    case ArchitectureRole: return e.architecture;
    case SampleRateRole: return e.sampleRate;
    case FramesRole: return e.frames;
    case ChannelsRole: return e.channels;
    case SizeRole: return e.size;
    case DetailRole: return detail(e);
    }
    return {};
}

QHash<int, QByteArray> Library::roleNames() const
{
    return {{IdRole, "id"}, {NameRole, "name"}, {GroupRole, "group"}, {MakeRole, "make"},
            {ModelRole, "model"}, {GearTypeRole, "gearType"}, {ArchitectureRole, "architecture"},
            {SampleRateRole, "sampleRate"}, {FramesRole, "frames"}, {ChannelsRole, "channels"},
            {SizeRole, "size"}, {DetailRole, "detail"}};
}

QString Library::detail(const LibraryEntry &e)
{
    QStringList parts;
    if (!e.make.isEmpty())
        parts << e.make;
    if (!e.model.isEmpty())
        parts << e.model;
    if (e.sampleRate > 0)
        parts << QStringLiteral("%1 kHz").arg(QLocale::c().toString(e.sampleRate / 1000.0, 'g', 4));
    if (e.kind == LibraryEntry::Ir && e.frames >= 0)
        parts << QStringLiteral("%1 frames").arg(QLocale::system().toString(e.frames));
    if (e.channels > 1)
        parts << QStringLiteral("%1 ch").arg(e.channels);
    if (e.kind == LibraryEntry::Model && !e.architecture.isEmpty())
        parts << e.architecture;
    parts << QLocale::system().formattedDataSize(e.size, 0, QLocale::DataSizeSIFormat);
    return parts.join(QStringLiteral(" · "));
}

LibraryEntry Library::read(LibraryEntry::Kind kind, const QString &root, const QString &path)
{
    LibraryEntry e;
    e.kind = kind;
    e.path = path;
    const QFileInfo info(path);
    const QString relative = QDir(root).relativeFilePath(path);
    e.id = relative.left(relative.size() - info.suffix().size() - 1);
    const qsizetype slash = e.id.lastIndexOf(u'/');
    e.group = slash < 0 ? QString() : e.id.left(slash);
    e.size = info.size();
    e.modified = info.lastModified();
    const QString stem = info.completeBaseName();
    QString claimedMake;
    if (info.suffix().compare(QLatin1String("nam"), Qt::CaseInsensitive) == 0) {
        const NamInfo nam = sniff::namFile(path);
        e.architecture = nam.architecture;
        e.sampleRate = nam.sampleRate;
        e.name = nam.name;
        e.model = nam.model;
        e.gearType = nam.gearType;
        claimedMake = nam.make;
        e.isIrNam = nam.architecture == QLatin1String(sniff::kIrArchitecture);
    } else if (const auto wav = sniff::wavFile(path)) {
        e.sampleRate = wav->sampleRate;
        e.channels = wav->channels;
        e.frames = wav->frames;
    }
    if (e.name.isEmpty())
        e.name = stem;
    e.make = make::guess(claimedMake, stem, e.group.isEmpty() ? QStringList() : e.group.split(u'/'));
    return e;
}

Library::ScanResult Library::scan(const QString &modelsRoot, const QString &irsRoot, const Cache &cache)
{
    ScanResult result;
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    auto walk = [&](LibraryEntry::Kind kind, const QString &root, QList<LibraryEntry> &out) {
        if (root.isEmpty())
            return;
        if (!QFileInfo(root).isDir()) {
            result.problem = QStringLiteral("no folder %1").arg(root);
            return;
        }
        const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
        // The engine's rules (namchain docs/assets.md): recursive, dot-names skipped, symlinks
        // followed only while they stay inside the root.
        QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (it.hasNext()) {
            const QString path = it.next();
            const QFileInfo info = it.fileInfo();
            if (QDir(root).relativeFilePath(path).split(u'/').constFirst().startsWith(u'.')
                || info.fileName().startsWith(u'.'))
                continue;
            if (path.contains(QLatin1String("/.")))
                continue;
            if (info.isSymLink() && !info.canonicalFilePath().startsWith(canonicalRoot + u'/'))
                continue;
            const QString suffix = info.suffix().toLower();
            const bool wanted = kind == LibraryEntry::Model ? suffix == QLatin1String("nam")
                                                            : (suffix == QLatin1String("wav") || suffix == QLatin1String("nam"));
            if (!wanted)
                continue;
            const auto cached = cache.constFind(path);
            if (cached != cache.constEnd() && cached->size == info.size() && cached->modified == info.lastModified())
                out.append(*cached);
            else
                out.append(read(kind, root, path));
        }
        std::sort(out.begin(), out.end(), [&](const LibraryEntry &a, const LibraryEntry &b) {
            if (a.group != b.group)
                return collator.compare(a.group, b.group) < 0;
            return collator.compare(a.name, b.name) < 0;
        });
    };
    walk(LibraryEntry::Model, modelsRoot, result.models);
    walk(LibraryEntry::Ir, irsRoot, result.irs);
    return result;
}

} // namespace rigpa
