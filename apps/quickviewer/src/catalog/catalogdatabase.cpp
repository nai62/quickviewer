#include <QtConcurrent>
#include <QtSql>
#include <QDebug>

#ifdef Q_OS_WIN
#    include <Windows.h>
#endif

#include "catalogbuilder.h"
#include "catalogdatabase.h"
#include "fileloader.h"
#include "volumenameparser.h"

namespace {

/** Catalog schema the application knows how to read. */
constexpr int SupportedCatalogSchema = 604;
/** Name of the catalog database the application ships as a resource. */
constexpr auto BundledCatalogDatabase = ":/databases/thumbnail_database";

/** One volume of t_volumes, as far as the ordering pass needs it. */
class VolumeOrder
{
public:
    int id;
    int parent_id;
    QString realname;
};

bool volumeOrderLessThan(const VolumeOrder &s1, const VolumeOrder &s2)
{
    return IFileLoader::caseInsensitiveLessThan(s1.realname, s2.realname);
}

/**
 * Name of the SQL connection one handle owns. A shared name would let a second
 * handle take the connection over from the first one, which Qt reports only as
 * a warning before the first handle stops working.
 */
QString connectionNameFor(const void *handle)
{
    return QStringLiteral("catalog-%1").arg(reinterpret_cast<quintptr>(handle), 0, 16);
}

/** One catalog folder waiting for its scan, and where the result is stored. */
class CatalogFolderJob
{
public:
    QString path;
    int volumeId;
    int subVolumeParentId;
    bool baseFolder;
    QStringList ancestors;
};

/** Identifies the folder behind any symlink or Windows junction in its path. */
QString folderIdentity(const QString &path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()),
                                      0,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      nullptr,
                                      OPEN_EXISTING,
                                      FILE_FLAG_BACKUP_SEMANTICS,
                                      nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info{};
        const bool found = GetFileInformationByHandle(handle, &info);
        CloseHandle(handle);
        if (found) {
            return QStringLiteral("%1:%2:%3")
                .arg(info.dwVolumeSerialNumber)
                .arg(info.nFileIndexHigh)
                .arg(info.nFileIndexLow);
        }
    }
#endif
    return QFileInfo(path).canonicalFilePath();
}

/** True once the batch \a canceled belongs to has been asked to stop. */
bool isCanceled(const QAtomicInt *canceled)
{
    return canceled && canceled->loadAcquire() != 0;
}

} // namespace

CatalogDatabase::CatalogDatabase(QObject *parent, QString dbpath)
    : QObject(parent),
      m_dbPath(dbpath),
      m_connectionName(connectionNameFor(this)),
      m_connectionRegistered(false),
      m_transaction(false),
      m_catalogWatcher(this),
      m_catalogCanceled(0),
      m_ready(false),
      m_volumesDirty(true)
{
}

CatalogDatabase::~CatalogDatabase()
{
    // The worker owns its connection, but still reads our cancellation flag.
    m_catalogCanceled.storeRelease(1);
    m_catalogWatcher.cancel();
    m_catalogWatcher.waitForFinished();
    closeDatabase();
}

void CatalogDatabase::closeDatabase()
{
    if (!m_connectionRegistered) {
        return;
    }
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
    m_connectionRegistered = false;
}

bool CatalogDatabase::ensureReady()
{
    if (m_ready) {
        return true;
    }
    m_errorMessage.clear();

    if (!m_connectionRegistered) {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        m_db.setDatabaseName(m_dbPath);
        m_connectionRegistered = true;
    }

    const QFileInfo file(m_dbPath);
    if (!file.exists()) {
        if (!writeBundledDatabase(file)) {
            m_errorMessage = tr("The catalog database could not be created: %1").arg(m_dbPath);
            return false;
        }
    } else if (!file.isFile()) {
        m_errorMessage = tr("The catalog database is not a file: %1").arg(m_dbPath);
        return false;
    }

    if (!m_db.open()) {
        m_errorMessage = tr("The catalog database could not be opened: %1 (%2)")
                             .arg(m_dbPath, m_db.lastError().text());
        return false;
    }

    QString problem;
    if (!hasCatalogSchema(&problem)) {
        m_errorMessage = tr("The catalog database cannot be used: %1 (%2)").arg(m_dbPath, problem);
        return false;
    }

    qDebug() << "catalog database:" << m_dbPath;
    m_ready = true;
    loadTags();
    return true;
}

bool CatalogDatabase::writeBundledDatabase(const QFileInfo &file)
{
    const QString path = file.absoluteFilePath();
    if (!file.dir().exists() && !QDir().mkpath(file.dir().absolutePath())) {
        return false;
    }
    QFile bundled(QString::fromLatin1(BundledCatalogDatabase));
    if (!bundled.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray contents = bundled.readAll();

    // Write beside the target and rename it into place: a half-written file
    // never becomes the catalog database, and a second instance that created
    // the file meanwhile wins instead of being overwritten.
    QTemporaryFile temporary(path + QStringLiteral(".XXXXXX"));
    if (!temporary.open()) {
        return false;
    }
    if (temporary.write(contents) != contents.size() || !temporary.flush()) {
        return false;
    }
    if (!temporary.rename(path)) {
        return QFile::exists(path);
    }
    temporary.setAutoRemove(false);
    return true;
}

bool CatalogDatabase::hasCatalogSchema(QString *problem)
{
    static const char *const tables[] = {"t_catalogs",
                                         "t_volumes",
                                         "t_files",
                                         "t_thumbnails",
                                         "t_volumeorders",
                                         "t_fileorders",
                                         "t_tags",
                                         "t_volumetags"};
    for (const char *table : tables) {
        QSqlQuery query(m_db);
        if (!query.exec(QStringLiteral("SELECT 1 FROM %1 LIMIT 1").arg(QLatin1String(table)))) {
            *problem = tr("%1 is missing").arg(QLatin1String(table));
            return false;
        }
    }
    QSqlQuery viewQuery(m_db);
    if (!viewQuery.exec(QStringLiteral("SELECT 1 FROM v_volumethm LIMIT 1"))) {
        *problem = tr("the volume view is missing");
        return false;
    }
    QSqlQuery versionQuery(m_db);
    if (versionQuery.exec(QStringLiteral("SELECT MAX(version) FROM t_version")) &&
        versionQuery.next() && versionQuery.value(0).toInt() > SupportedCatalogSchema) {
        *problem = tr("the database comes from a newer QuickViewer");
        return false;
    }
    return true;
}

int CatalogDatabase::buildCatalogVolumes(const QString &dirpath,
                                         int catalog_id,
                                         const QAtomicInt *canceled)
{
    int volume_id = createVolume(dirpath, catalog_id, -1);
    if (volume_id < 0) {
        return -1;
    }

    QSqlQuery t_files(m_db);
    t_files.prepare(
        "INSERT INTO t_files "
        "(volume_id,name,size,width,height,thumb_id,created_at,updated_at,alternated)"
        " VALUES "
        "(:volume_id,:name,:size,:width,:height,:thumb_id,:created_at,:updated_at,:alternated)");
    QSqlQuery t_fileorders(m_db);
    t_fileorders.prepare("INSERT INTO t_fileorders (id,volume_id,filename_asc)"
                         " VALUES (:id,:volume_id,:filename_asc)");
    QSqlQuery t_thumbs(m_db);
    t_thumbs.prepare("INSERT INTO t_thumbnails (width,height,thumbnail,created_at)"
                     " VALUES (:width,:height,:thumbnail,:created_at)");
    QSqlQuery t_volumes(m_db);
    t_volumes.prepare(
        "UPDATE t_volumes SET frontpage_id=:frontpage_id, thumb_id=:thumb_id WHERE id=:id");

    // The base folder is a volume of its own, and the rows of its sub-volumes
    // keep the parent id this traversal has always recorded for them.
    QList<CatalogFolderJob> jobs;
    jobs << CatalogFolderJob{dirpath, volume_id, -1, true, {folderIdentity(dirpath)}};
    int scannedCount = 0;
    int knownCount = jobs.size();
    emit catalogProgressRangeChanged(0, knownCount);

    while (!jobs.isEmpty()) {
        QList<QFuture<CatalogFolderScan>> scans;
        scans.reserve(jobs.size());
        for (const CatalogFolderJob &job : jobs) {
            // The scan runs on a worker thread, so it owns its arguments.
            const QString path = job.path;
            const bool baseFolder = job.baseFolder;
            scans << QtConcurrent::run(
                [path, baseFolder] { return CatalogBuilder::scanFolder(path, baseFolder); });
        }

        QList<CatalogFolderJob> subJobs;
        for (int i = 0; i < scans.size(); i++) {
            if (isCanceled(canceled)) {
                return -1;
            }
            const CatalogFolderJob job = jobs.at(i);
            const CatalogFolderScan scan = scans.at(i).result();
            const QDir dir(job.path);
            for (const QString &name : scan.subVolumeNames) {
                const QString path = dir.filePath(name);
                const QString canonical = folderIdentity(path);
                // Follow linked folders, but never return to an ancestor through
                // a symlink or Windows junction.
                if (canonical.isEmpty() || job.ancestors.contains(canonical)) {
                    continue;
                }
                const int subVolumeId = createVolume(path, catalog_id, job.subVolumeParentId);
                if (subVolumeId < 0) {
                    return -1;
                }
                // Kept from the traversal this replaces: a volume is recorded
                // under the folder above the one that holds it.
                subJobs << CatalogFolderJob{
                    path, subVolumeId, job.volumeId, false, job.ancestors + QStringList{canonical}};
            }

            if (!scan.cover.isEmpty()) {
                emit catalogProgressTextChanged(QFileInfo(job.path).fileName());

                t_thumbs.bindValue(":width", scan.cover.thumbnailSize.width());
                t_thumbs.bindValue(":height", scan.cover.thumbnailSize.height());
                t_thumbs.bindValue(":thumbnail", scan.cover.thumbnail);
                t_thumbs.bindValue(":created_at", QDateTime::currentDateTime());
                if (!execQuery(t_thumbs, "t_thumbs")) {
                    return -1;
                }

                t_files.bindValue(":volume_id", job.volumeId);
                t_files.bindValue(":name", scan.cover.name);
                t_files.bindValue(":size", scan.cover.size);
                t_files.bindValue(":width", scan.cover.imageSize.width());
                t_files.bindValue(":height", scan.cover.imageSize.height());
                t_files.bindValue(":thumb_id", t_thumbs.lastInsertId());
                t_files.bindValue(":updated_at", scan.cover.updated);
                if (!execQuery(t_files, "t_files")) {
                    return -1;
                }

                t_fileorders.bindValue(":volume_id", job.volumeId);
                t_fileorders.bindValue(":id", t_files.lastInsertId());
                t_fileorders.bindValue(":filename_asc", 0);
                if (!execQuery(t_fileorders, "t_fileorders")) {
                    return -1;
                }

                t_volumes.bindValue(":frontpage_id", t_files.lastInsertId());
                t_volumes.bindValue(":thumb_id", t_thumbs.lastInsertId());
                t_volumes.bindValue(":id", job.volumeId);
                if (!execQuery(t_volumes, "t_volumes")) {
                    return -1;
                }
            }

            emit catalogProgressValueChanged(++scannedCount);
        }

        knownCount += subJobs.size();
        emit catalogProgressRangeChanged(0, knownCount);
        jobs = subJobs;
    }

    return volume_id;
}

int CatalogDatabase::createVolume(const QString &dirpath, int catalog_id, int parent_id)
{
    const QFileInfo info(dirpath);
    const QString realname = info.fileName();
    qDebug() << "volume: " << realname;
    emit catalogProgressTextChanged(realname);

    const TaggedName tagged = parseVolumeName(realname);

    QSqlQuery t_volumes(m_db);
    t_volumes.prepare("INSERT INTO t_volumes (name, realname, path, catalog_id, parent_id) VALUES "
                      "(:name, :realname,:path,:catalog_id,:parent_id)");
    t_volumes.bindValue(":name", tagged.name);
    t_volumes.bindValue(":realname", realname);
    t_volumes.bindValue(":path", QDir::toNativeSeparators(dirpath));
    t_volumes.bindValue(":catalog_id", catalog_id);
    t_volumes.bindValue(":parent_id", parent_id);
    if (!execQuery(t_volumes, "t_volumes")) {
        return -1;
    }
    int volume_id = t_volumes.lastInsertId().toInt();

    QSqlQuery t_tags(m_db);
    t_tags.prepare("INSERT INTO t_tags (name, type_id) VALUES (:name, :type_id)");
    QSqlQuery t_tagentries(m_db);
    t_tagentries.prepare("INSERT INTO t_volumetags (volume_id, tag_id, catalog_id) VALUES "
                         "(:volume_id, :tag_id, :catalog_id)");
    QSet<int> storedTags;
    for (const TagRecord &t : tagged.tags) {
        QString tagkey = QString("%1:%2").arg(t.type_id).arg(t.name.toLower());
        if (!m_tags.contains(tagkey)) {
            t_tags.bindValue(":name", t.name);
            t_tags.bindValue(":type_id", t.type_id);
            if (!execQuery(t_tags, "t_tags")) {
                return -1;
            }
            TagRecord newtag(t.name, t.type_id);
            newtag.id = t_tags.lastInsertId().toInt();
            newtag.nameNoCase = t.name.toLower();
            m_tags[tagkey] = newtag;
            m_tags2[newtag.id] = &m_tags[tagkey];
        }
        TagRecord &tag = m_tags[tagkey];
        if (storedTags.contains(tag.id)) {
            continue;
        }
        storedTags.insert(tag.id);
        t_tagentries.bindValue(":volume_id", volume_id);
        t_tagentries.bindValue(":tag_id", tag.id);
        t_tagentries.bindValue(":catalog_id", catalog_id);
        if (!execQuery(t_tagentries, "t_volumetags")) {
            return -1;
        }
    }

    return volume_id;
}

bool CatalogDatabase::updateVolumeOrders()
{
    QSqlQuery t_volumeorders(m_db);
    t_volumeorders.prepare("DELETE FROM t_volumeorders");
    if (!execQuery(t_volumeorders, "t_volumeorders")) {
        return false;
    }

    QSqlQuery t_volumes(m_db);
    t_volumes.prepare("SELECT id, parent_id, realname FROM t_volumes");
    if (!execQuery(t_volumes, "t_volumes")) {
        return false;
    }

    QList<VolumeOrder> volumes;
    while (t_volumes.next()) {
        VolumeOrder order;
        order.id = t_volumes.value("id").toInt();
        order.parent_id = t_volumes.value("parent_id").toInt();
        order.realname = t_volumes.value("realname").toString().toLower();
        volumes << order;
    }
    std::sort(volumes.begin(), volumes.end(), volumeOrderLessThan);

    t_volumeorders.prepare("INSERT INTO t_volumeorders (id,parent_id,volumename_asc)"
                           " VALUES (:id,:parent_id,:volumename_asc)");
    for (int i = 0; i < volumes.size(); i++) {
        t_volumeorders.bindValue(":id", volumes[i].id);
        t_volumeorders.bindValue(":parent_id", volumes[i].parent_id);
        t_volumeorders.bindValue(":volumename_asc", i);
        if (!execQuery(t_volumeorders, "t_volumeorders")) {
            return false;
        }
    }
    return true;
}

CatalogRecord CatalogDatabase::createCatalog(QString name, QString path)
{
    return createCatalog(name, path, nullptr);
}

CatalogRecord CatalogDatabase::createCatalog(QString name, QString path, const QAtomicInt *canceled)
{
    CatalogRecord catalog = {0};
    if (!ensureReady()) {
        return catalog;
    }
    const QFileInfo source(path);
    if (path.trimmed().isEmpty() ||
        !(source.isDir() || (source.isFile() && IFileLoader::isArchiveFile(path)))) {
        m_errorMessage = tr("The catalog source is not a folder or archive: %1").arg(path);
        return catalog;
    }
    path = source.absoluteFilePath();
    catalog.name = name;
    catalog.path = path;
    catalog.created_at = QDateTime::currentDateTime();

    if (!transaction()) {
        return catalog;
    }

    QSqlQuery t_catalogs(m_db);
    t_catalogs.prepare("INSERT INTO t_catalogs (name,path,created_at,updated_at)"
                       " VALUES (:name,:path,:created_at,:updated_at)");
    t_catalogs.bindValue(":name", name);
    t_catalogs.bindValue(":path", QDir::toNativeSeparators(path));
    t_catalogs.bindValue(":created_at", catalog.created_at);
    t_catalogs.bindValue(":updated_at", catalog.created_at);
    if (!execQuery(t_catalogs, "t_catalogs")) {
        rollback();
        return catalog;
    }

    int catalog_id = catalog.id = t_catalogs.lastInsertId().toInt();
    int basevolume_id = buildCatalogVolumes(path, catalog_id, canceled);
    if (basevolume_id > 0) {
        t_catalogs.prepare("UPDATE t_catalogs SET basevolume_id=:basevolume_id WHERE id=:id");
        t_catalogs.bindValue(":basevolume_id", basevolume_id);
        t_catalogs.bindValue(":id", catalog_id);
        if (!execQuery(t_catalogs, "t_catalogs")) {
            rollback();
            return catalog;
        }
    } else {
        // Nothing of the catalog was built, so none of it is stored.
        rollback();
        return catalog;
    }

    if (isCanceled(canceled)) {
        rollback();
        return catalog;
    }
    // The catalog view reads the volume order, so the catalog is complete only
    // once the volumes it just built have one.
    if (!updateVolumeOrders()) {
        rollback();
        return catalog;
    }
    if (!commit()) {
        return catalog;
    }
    catalog.basevolume_id = basevolume_id;
    catalog.updated_at = catalog.created_at;
    catalog.created = true;
    m_volumesDirty = true;
    emit catalogCreated(catalog);

    return catalog;
}

QFutureWatcher<QList<CatalogRecord>> *
CatalogDatabase::createCatalogAsync(QList<CatalogRecord> newers)
{
    if (m_catalogWatcher.isRunning()) {
        return &m_catalogWatcher;
    }
    m_volumesDirty = true;
    m_errorMessage.clear();
    m_catalogCanceled.storeRelease(0);
    if (!ensureReady()) {
        // Nothing can be stored; finish an empty batch so that the caller
        // still hears that the build ended.
        newers.clear();
    }
    const QString path = m_dbPath;
    const QString openingError = m_errorMessage;
    QFuture<QList<CatalogRecord>> future = QtConcurrent::run([this, path, newers, openingError] {
        // Create, use and close this connection on the worker that owns it.
        CatalogDatabase worker(nullptr, path);
        connect(&worker,
                &CatalogDatabase::catalogCreated,
                this,
                &CatalogDatabase::catalogCreated,
                Qt::DirectConnection);
        connect(&worker,
                &CatalogDatabase::catalogProgressRangeChanged,
                this,
                &CatalogDatabase::catalogProgressRangeChanged,
                Qt::DirectConnection);
        connect(&worker,
                &CatalogDatabase::catalogProgressValueChanged,
                this,
                &CatalogDatabase::catalogProgressValueChanged,
                Qt::DirectConnection);
        connect(&worker,
                &CatalogDatabase::catalogProgressTextChanged,
                this,
                &CatalogDatabase::catalogProgressTextChanged,
                Qt::DirectConnection);
        QList<CatalogRecord> result;
        for (const CatalogRecord &request : newers) {
            if (isCanceled(&m_catalogCanceled)) {
                break;
            }
            result << worker.createCatalog(request.name, request.path, &m_catalogCanceled);
        }
        const QString error = openingError.isEmpty() ? worker.errorMessage() : openingError;
        QMetaObject::invokeMethod(
            this,
            [this, error] {
                m_errorMessage = error;
                m_volumesDirty = true;
                loadTags();
            },
            Qt::QueuedConnection);
        return result;
    });
    m_catalogWatcher.setFuture(future);
    return &m_catalogWatcher;
}

void CatalogDatabase::cancelCreateCatalogAsync()
{
    m_catalogCanceled.storeRelease(1);
    m_catalogWatcher.cancel();
}

QMap<int, CatalogRecord> CatalogDatabase::catalogs()
{
    QMap<int, CatalogRecord> result;
    if (!ensureReady()) {
        return result;
    }
    QSqlQuery t_catalogs(m_db);
    t_catalogs.prepare("SELECT * FROM t_catalogs");
    if (!execQuery(t_catalogs, "t_catalogs")) {
        return result;
    }
    while (t_catalogs.next()) {
        CatalogRecord cr;
        cr.id = t_catalogs.value("id").toInt();
        cr.basevolume_id = t_catalogs.value("basevolume_id").toInt();
        cr.name = t_catalogs.value("name").toString();
        cr.path = t_catalogs.value("path").toString();
        cr.created_at = t_catalogs.value("created_at").toDateTime();
        cr.created = true;
        result[cr.id] = cr;
    }
    return result;
}

QList<VolumeThumbRecord> CatalogDatabase::volumes()
{
    if (!ensureReady()) {
        return QList<VolumeThumbRecord>();
    }
    if (!m_volumesDirty) {
        return m_volumesCache;
    }
    QList<VolumeThumbRecord> result;
    QSqlQuery v_volumethm(m_db);
    // The view carries the cover itself but not the row it came from, and the
    // cover of a volume is read and fitted once per row of the catalog list.
    v_volumethm.prepare("SELECT v_volumethm.*, t_volumes.thumb_id AS thumb_id, "
                        "t_volumes.catalog_id AS catalog_id FROM v_volumethm "
                        "LEFT OUTER JOIN t_volumes ON t_volumes.id = v_volumethm.id ORDER BY "
                        "v_volumethm.realname_asc");
    if (!execQuery(v_volumethm, "v_volumethm")) {
        // The query failed, so the cache stays empty rather than remembering
        // the failure as the contents of the catalog.
        return result;
    }
    QMap<int, int> rowOfVolume;
    while (v_volumethm.next()) {
        VolumeThumbRecord vtr;
        vtr.id = v_volumethm.value("id").toInt();
        vtr.name = v_volumethm.value("name").toString();
        vtr.nameNoCase = vtr.name.toLower();
        vtr.realname = v_volumethm.value("realname").toString();
        vtr.realnameNoCase = vtr.realname.toLower();
        vtr.path = v_volumethm.value("path").toString();
        vtr.frontpage_id = v_volumethm.value("frontpage_id").toInt();
        vtr.parent_id = v_volumethm.value("parent_id").toInt();
        vtr.thumb_id = v_volumethm.value("thumb_id").toInt();
        vtr.thumbnail = v_volumethm.value("thumbnail").toByteArray();
        vtr.catalog_id = v_volumethm.value("catalog_id").toInt();
        rowOfVolume.insert(vtr.id, int(result.size()));
        result.append(vtr);
    }
    QSqlQuery tags(m_db);
    tags.prepare(QStringLiteral("SELECT v.volume_id, t.name FROM t_volumetags v "
                                "JOIN t_tags t ON t.id = v.tag_id"));
    if (!execQuery(tags, "volume tags")) {
        return {};
    }
    while (tags.next()) {
        const auto row = rowOfVolume.constFind(tags.value(0).toInt());
        if (row != rowOfVolume.constEnd()) {
            result[row.value()].tags << tags.value(1).toString();
        }
    }
    loadTags();
    m_volumesDirty = false;
    return m_volumesCache = result;
}

QList<QPair<VolumeThumbRecord, QStringList>> CatalogDatabase::catalogVolumes(int catalog_id)
{
    QList<QPair<VolumeThumbRecord, QStringList>> result;
    if (!ensureReady()) {
        return result;
    }

    QSqlQuery volumes(m_db);
    volumes.prepare("SELECT t_volumes.id, t_volumes.name, t_volumes.realname, t_volumes.path, "
                    "t_volumes.frontpage_id, t_volumes.parent_id, t_volumes.catalog_id, "
                    "t_volumes.thumb_id, t_thumbnails.thumbnail FROM t_volumes "
                    "LEFT OUTER JOIN t_thumbnails ON t_volumes.thumb_id = t_thumbnails.id "
                    "WHERE t_volumes.catalog_id = :catalog_id ORDER BY t_volumes.realname");
    volumes.bindValue(":catalog_id", catalog_id);
    if (!volumes.exec()) {
        qDebug() << "t_volumes of catalog query failed: " << volumes.lastError();
        return result;
    }
    QMap<int, int> rowOfVolume;
    while (volumes.next()) {
        VolumeThumbRecord record;
        record.id = volumes.value(0).toInt();
        record.name = volumes.value(1).toString();
        record.nameNoCase = record.name.toLower();
        record.realname = volumes.value(2).toString();
        record.realnameNoCase = record.realname.toLower();
        record.path = volumes.value(3).toString();
        record.frontpage_id = volumes.value(4).toInt();
        record.parent_id = volumes.value(5).toInt();
        record.catalog_id = volumes.value(6).toInt();
        record.thumb_id = volumes.value(7).toInt();
        record.thumbnail = volumes.value(8).toByteArray();
        rowOfVolume.insert(record.id, int(result.size()));
        result.append({record, QStringList()});
    }

    QSqlQuery tags(m_db);
    tags.prepare("SELECT t_volumetags.volume_id, t_tags.name FROM t_volumetags "
                 "INNER JOIN t_tags ON t_tags.id = t_volumetags.tag_id "
                 "WHERE t_volumetags.catalog_id = :catalog_id "
                 "ORDER BY t_volumetags.volume_id, t_tags.name");
    tags.bindValue(":catalog_id", catalog_id);
    if (!tags.exec()) {
        qDebug() << "t_volumetags of catalog query failed: " << tags.lastError();
        return result;
    }
    while (tags.next()) {
        const auto row = rowOfVolume.constFind(tags.value(0).toInt());
        if (row != rowOfVolume.constEnd()) {
            result[row.value()].second << tags.value(1).toString();
        }
    }
    return result;
}

QStringList CatalogDatabase::missingVolumePaths()
{
    QStringList missing;
    if (!ensureReady()) {
        return missing;
    }
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT path FROM t_volumes ORDER BY id"))) {
        qDebug() << "t_volumes query failed: " << query.lastError();
        return missing;
    }
    while (query.next()) {
        const QString path = query.value(0).toString();
        if (!QFileInfo::exists(path) && !missing.contains(path)) {
            missing << path;
        }
    }
    return missing;
}

int CatalogDatabase::removeMissingVolumes()
{
    const QStringList missing = missingVolumePaths();
    if (missing.isEmpty() || !ensureReady()) {
        return 0;
    }

    // Everything that hangs off a volume, then the volume itself. The
    // statements run inside one transaction, so a failure leaves the catalog
    // as it was.
    static const char *const removals[] = {
        "DELETE FROM t_thumbnails WHERE id IN (SELECT thumb_id FROM t_files WHERE volume_id IN "
        "(SELECT id FROM t_volumes WHERE path = :path))",
        "DELETE FROM t_files WHERE volume_id IN (SELECT id FROM t_volumes WHERE path = :path)",
        "DELETE FROM t_fileorders WHERE volume_id IN (SELECT id FROM t_volumes WHERE path = :path)",
        "DELETE FROM t_volumeorders WHERE id IN (SELECT id FROM t_volumes WHERE path = :path)",
        "DELETE FROM t_volumetags WHERE volume_id IN (SELECT id FROM t_volumes WHERE path = :path)",
        "DELETE FROM t_volumes WHERE path = :path",
    };

    if (!transaction()) {
        return 0;
    }
    for (const char *statement : removals) {
        for (const QString &path : missing) {
            QSqlQuery query(m_db);
            query.prepare(QString::fromLatin1(statement));
            query.bindValue(":path", path);
            if (!execQuery(query, "remove missing volumes")) {
                rollback();
                return 0;
            }
        }
    }
    if (!updateVolumeOrders()) {
        rollback();
        return 0;
    }
    if (!removeUnusedTags()) {
        rollback();
        return 0;
    }
    if (!commit()) {
        return 0;
    }
    loadTags();
    m_volumesDirty = true;
    return missing.size();
}

bool CatalogDatabase::setVolumeTags(int volume_id, const QStringList &tags)
{
    return editVolume(volume_id, tags, nullptr);
}

bool CatalogDatabase::setVolumeDetails(int volume_id, const QString &name, const QStringList &tags)
{
    return editVolume(volume_id, tags, &name);
}

bool CatalogDatabase::editVolume(int volume_id, const QStringList &tags, const QString *name)
{
    if (!ensureReady()) {
        return false;
    }

    int catalog_id = -1;
    {
        QSqlQuery volume(m_db);
        volume.prepare(QStringLiteral("SELECT catalog_id FROM t_volumes WHERE id = :id"));
        volume.bindValue(":id", volume_id);
        if (!volume.exec() || !volume.next()) {
            qDebug() << "t_volumes lookup failed: " << volume.lastError();
            return false;
        }
        catalog_id = volume.value(0).toInt();
    }

    // The names the user asked for, without blanks and without repeats.
    QStringList wanted;
    for (const QString &tag : tags) {
        const QString name = tag.trimmed();
        if (!name.isEmpty() && !wanted.contains(name, Qt::CaseInsensitive)) {
            wanted << name;
        }
    }

    if (!transaction()) {
        return false;
    }
    QSqlQuery removal(m_db);
    removal.prepare(QStringLiteral("DELETE FROM t_volumetags WHERE volume_id = :volume_id"));
    removal.bindValue(":volume_id", volume_id);
    if (!execQuery(removal, "t_volumetags")) {
        rollback();
        return false;
    }
    for (const QString &name : wanted) {
        const int tag_id = findOrCreateTag(name);
        if (tag_id < 0) {
            rollback();
            return false;
        }
        QSqlQuery entry(m_db);
        entry.prepare(QStringLiteral("INSERT INTO t_volumetags (volume_id, tag_id, catalog_id)"
                                     " VALUES (:volume_id, :tag_id, :catalog_id)"));
        entry.bindValue(":volume_id", volume_id);
        entry.bindValue(":tag_id", tag_id);
        entry.bindValue(":catalog_id", catalog_id);
        if (!execQuery(entry, "t_volumetags")) {
            rollback();
            return false;
        }
    }
    if ((name && !setVolumeDisplayName(volume_id, *name)) || !removeUnusedTags()) {
        rollback();
        return false;
    }
    if (!commit()) {
        return false;
    }

    // The catalog and the tag bar read the tags from memory as well.
    loadTags();
    m_volumesDirty = true;
    return true;
}

bool CatalogDatabase::removeUnusedTags()
{
    QSqlQuery unused(m_db);
    unused.prepare(QStringLiteral("DELETE FROM t_tags WHERE NOT EXISTS "
                                  "(SELECT 1 FROM t_volumetags WHERE tag_id = t_tags.id)"));
    return execQuery(unused, "unused tags");
}

int CatalogDatabase::findOrCreateTag(const QString &name)
{
    QSqlQuery tags(m_db);
    if (!tags.exec(QStringLiteral("SELECT id, name FROM t_tags"))) {
        qDebug() << "t_tags query failed: " << tags.lastError();
        return -1;
    }
    while (tags.next()) {
        if (QString::compare(tags.value(1).toString(), name, Qt::CaseInsensitive) == 0) {
            return tags.value(0).toInt();
        }
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral("INSERT INTO t_tags (name, type_id) VALUES (:name, 0)"));
    insert.bindValue(":name", name);
    if (!execQuery(insert, "t_tags")) {
        return -1;
    }
    return insert.lastInsertId().toInt();
}

bool CatalogDatabase::setVolumeDisplayName(int volume_id, const QString &name)
{
    if (!ensureReady()) {
        return false;
    }
    QSqlQuery volume(m_db);
    volume.prepare(QStringLiteral("UPDATE t_volumes SET name = :name WHERE id = :id"));
    volume.bindValue(":name", name);
    volume.bindValue(":id", volume_id);
    if (!execQuery(volume, "t_volumes")) {
        return false;
    }
    m_volumesDirty = true;
    return true;
}

void CatalogDatabase::loadTags()
{
    if (!ensureReady()) {
        return;
    }
    QSqlQuery t_tags(m_db);
    if (!t_tags.exec("SELECT * FROM t_tags ORDER BY id")) {
        qDebug() << "t_tags query failed: " << t_tags.lastError();
        return;
    }
    m_tags.clear();
    m_tags2.clear();
    while (t_tags.next()) {
        TagRecord tag;
        tag.id = t_tags.value("id").toInt();
        tag.name = t_tags.value("name").toString();
        tag.type_id = t_tags.value("type_id").toInt();
        QString tagkey = QString("%1:%2").arg(tag.type_id).arg(tag.name.toLower());
        m_tags[tagkey] = tag;
        m_tags2[tag.id] = &m_tags[tagkey];
    }
}

QMap<int, TagRecord *> CatalogDatabase::tagsByCount()
{
    if (!ensureReady()) {
        return QMap<int, TagRecord *>();
    }
    QSqlQuery t_tags(m_db);
    const bool queried =
        t_tags.exec("SELECT t.id, t.name, t.type_id, v2.cnt FROM t_tags t INNER JOIN "
                    "(SELECT COUNT(*) as cnt, v.tag_id FROM t_volumetags v GROUP BY v.tag_id) v2 "
                    "ON v2.tag_id = t.id "
                    "ORDER BY v2.cnt DESC");
    if (!queried) {
        qDebug() << "t_tags by count query failed: " << t_tags.lastError();
        return QMap<int, TagRecord *>();
    }
    QMap<int, TagRecord *> result;
    int cnt = 0;
    while (t_tags.next()) {
        const int tag_id = t_tags.value("id").toInt();
        const auto tag = m_tags2.constFind(tag_id);
        if (tag == m_tags2.constEnd() || !tag.value()) {
            continue;
        }
        tag.value()->count = t_tags.value("cnt").toInt();
        result[cnt++] = tag.value();
    }
    return result;
}

QList<TagRecord> CatalogDatabase::getTagsFromVolumeId(int volume_id)
{
    QList<TagRecord> result;
    if (!ensureReady()) {
        return result;
    }
    QSqlQuery t_tags(m_db);
    t_tags.prepare("SELECT t.id, t.name, t.type_id FROM t_tags t "
                   "WHERE t.id IN (SELECT tag_id FROM t_volumetags WHERE volume_id=:volume_id)");
    t_tags.bindValue(":volume_id", volume_id);
    if (!execQuery(t_tags, "t_tags")) {
        return result;
    }

    while (t_tags.next()) {
        TagRecord tag;
        tag.id = t_tags.value("id").toInt();
        tag.name = t_tags.value("name").toString();
        tag.type_id = t_tags.value("type_id").toInt();
        result << tag;
    }
    return result;
}

bool CatalogDatabase::deleteCatalog(int id)
{
    if (!ensureReady()) {
        return false;
    }
    if (!transaction()) {
        return false;
    }

    QSqlQuery t_thumbs(m_db);
    t_thumbs.prepare("DELETE FROM t_thumbnails WHERE id IN (SELECT thumb_id FROM t_files WHERE "
                     "volume_id IN (SELECT id FROM t_volumes WHERE catalog_id=:catalog_id))");
    t_thumbs.bindValue(":catalog_id", id);
    if (!execQuery(t_thumbs, "t_thumbnails")) {
        rollback();
        return false;
    }

    QSqlQuery t_files(m_db);
    t_files.prepare("DELETE FROM t_files WHERE volume_id IN (SELECT id FROM t_volumes WHERE "
                    "catalog_id=:catalog_id)");
    t_files.bindValue(":catalog_id", id);
    if (!execQuery(t_files, "t_files")) {
        rollback();
        return false;
    }

    QSqlQuery t_fileorders(m_db);
    t_fileorders.prepare("DELETE FROM t_fileorders WHERE volume_id IN (SELECT id FROM t_volumes "
                         "WHERE catalog_id=:catalog_id)");
    t_fileorders.bindValue(":catalog_id", id);
    if (!execQuery(t_fileorders, "t_fileorders")) {
        rollback();
        return false;
    }

    QSqlQuery t_volumeorders(m_db);
    t_volumeorders.prepare("DELETE FROM t_volumeorders WHERE id IN (SELECT id FROM t_volumes WHERE "
                           "catalog_id=:catalog_id)");
    t_volumeorders.bindValue(":catalog_id", id);
    if (!execQuery(t_volumeorders, "t_volumeorders")) {
        rollback();
        return false;
    }

    QSqlQuery t_volumetags(m_db);
    t_volumetags.prepare("DELETE FROM t_volumetags WHERE catalog_id=:catalog_id");
    t_volumetags.bindValue(":catalog_id", id);
    if (!execQuery(t_volumetags, "t_volumetags")) {
        rollback();
        return false;
    }

    QSqlQuery t_volumes(m_db);
    t_volumes.prepare("DELETE FROM t_volumes WHERE catalog_id=:catalog_id");
    t_volumes.bindValue(":catalog_id", id);
    if (!execQuery(t_volumes, "t_volumes")) {
        rollback();
        return false;
    }

    QSqlQuery t_catalogs(m_db);
    t_catalogs.prepare("DELETE FROM t_catalogs WHERE id=:id");
    t_catalogs.bindValue(":id", id);
    if (!execQuery(t_catalogs, "t_catalogs")) {
        rollback();
        return false;
    }

    if (!removeUnusedTags()) {
        rollback();
        return false;
    }
    if (!commit()) {
        return false;
    }
    loadTags();
    m_volumesDirty = true;
    return true;
}

bool CatalogDatabase::updateCatalogName(int id, QString name)
{
    if (!ensureReady()) {
        return false;
    }
    QSqlQuery t_catalogs(m_db);
    t_catalogs.prepare("UPDATE t_catalogs SET name=:name, updated_at=:updated_at WHERE id=:id");
    t_catalogs.bindValue(":id", id);
    t_catalogs.bindValue(":name", name);
    t_catalogs.bindValue(":updated_at", QDateTime::currentDateTime());
    return execQuery(t_catalogs, "t_catalogs");
}

bool CatalogDatabase::deleteAllCatalogs()
{
    if (!ensureReady()) {
        return false;
    }
    if (!transaction()) {
        return false;
    }
    static const char *const removals[] = {"t_thumbnails",
                                           "t_fileorders",
                                           "t_volumeorders",
                                           "t_files",
                                           "t_volumes",
                                           "t_tags",
                                           "t_volumetags",
                                           "t_catalogs"};
    for (const char *table : removals) {
        QSqlQuery removal(m_db);
        if (!removal.exec(QStringLiteral("DELETE FROM %1").arg(QLatin1String(table)))) {
            m_errorMessage = removal.lastError().text();
            qDebug() << table << " delete failed: " << removal.lastError();
            rollback();
            return false;
        }
    }
    if (!commit()) {
        return false;
    }
    m_volumesDirty = true;
    loadTags();
    vacuum();
    return true;
}

bool CatalogDatabase::transaction()
{
    if (m_transaction) {
        m_errorMessage = tr("A catalog transaction is already running.");
        return false;
    }
    if (!m_db.transaction()) {
        m_errorMessage = m_db.lastError().text();
        qDebug() << "m_db transaction failed: " << m_db.lastError();
        return false;
    }
    m_errorMessage.clear();
    m_transaction = true;
    return true;
}

bool CatalogDatabase::commit()
{
    if (!m_transaction) {
        return false;
    }
    if (!m_db.commit()) {
        m_errorMessage = m_db.lastError().text();
        qDebug() << "m_db commit failed: " << m_db.lastError();
        rollback();
        return false;
    }
    m_transaction = false;
    return true;
}

void CatalogDatabase::rollback()
{
    if (!m_transaction) {
        return;
    }
    if (!m_db.rollback()) {
        qDebug() << "m_db rollback failed: " << m_db.lastError();
        return;
    }
    m_transaction = false;
    m_volumesDirty = true;
    // Inserts rolled back above may have allocated tag ids in memory.
    loadTags();
}

void CatalogDatabase::vacuum()
{
    if (!ensureReady()) {
        return;
    }
    QSqlQuery vacuumQuery(m_db);
    if (!vacuumQuery.exec("VACUUM")) {
        qDebug() << "VACUUM failed: " << vacuumQuery.lastError();
    }
}

bool CatalogDatabase::execQuery(QSqlQuery &query, const QString &statement)
{
    if (!query.exec()) {
        m_errorMessage = query.lastError().text();
        qDebug() << statement << " query failed: " << query.lastError();
        return false;
    }
    return true;
}
