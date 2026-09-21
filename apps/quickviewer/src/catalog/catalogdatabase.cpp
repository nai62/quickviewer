#include <QtGui>
#include <QtSql>
#include <QDebug>
#include <QApplication>

#include "catalogdatabase.h"
#include "fileloader.h"
#include "volume.h"
#include "volumeloader.h"

namespace {

// Width of the thumbnails stored in the catalog database.
constexpr int ThumbnailWidth = 96;

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

} // namespace

QString CatalogDatabase::DateTimeToIsoString(QDateTime datetime)
{
    return datetime.toString(QStringLiteral("yyyy/MM/dd hh:mm:ss"));
}

CatalogDatabase::CatalogDatabase(QObject *parent, QString dbpath)
    : QObject(parent),
      m_connectionName(connectionNameFor(this)),
      m_transaction(false),
      m_catalogWatcher(this),
      m_catalogCanceled(0),
      m_volumesDirty(true)
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbpath);
    if (!m_db.open()) {
        qDebug() << "Error: connection with database fail" << dbpath;
    } else {
        qDebug() << "Database: connection ok";
    }
}

CatalogDatabase::~CatalogDatabase()
{
    // A catalog build can still be running on a worker thread, and it talks to
    // the connection that is unregistered here.
    m_catalogCanceled.storeRelease(1);
    m_catalogWatcher.cancel();
    m_catalogWatcher.waitForFinished();
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

VolumeWorker
CatalogDatabase::createSubVolumesConcurrent(QString dirpath, int volume_id, int parent_id)
{
    VolumeWorker vw = {0};
    vw.frontPage.asc = -1;
    vw.dirpath = dirpath;
    vw.volume_id = volume_id;
    vw.parent_id = parent_id;

    if (IFileLoader::isArchiveFile(dirpath)) {
        VolumeLoader volumeLoader(dirpath);
        ImageContent thumbnailContent = volumeLoader.loadThumbnailSourceImage();
        if (!thumbnailContent.loadedImage.isNull()) {
            vw.frontPage = createFileRecordFromArchive(dirpath, thumbnailContent, 0);
        }
        return vw;
    }

    QDir dir(dirpath);
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Unsorted);
    IFileLoader::sortFiles(subdirs);
    vw.subpaths = subdirs;

    QStringList files = dir.entryList(QDir::Files, QDir::Unsorted);
    IFileLoader::sortFiles(files);
    for (const QString &filename : files) {
        if (isCatalogCreationCanceled()) {
            break;
        }
        if (!IFileLoader::isImageFile(filename)) {
            continue;
        }
        vw.frontPage = createFileRecord(filename, dir.filePath(filename), 0);
        break; // ONLY FRONT PAGE
    }
    return vw;
}

int CatalogDatabase::createVolumesFrontPageOnly(QString dirpath, int catalog_id)
{
    int volume_id = createVolumeInternal(dirpath, catalog_id, -1);
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

    QList<VolumeWorker> parentworkers;
    {
        QDir dir(dirpath);
        VolumeWorker root = {0};
        root.dirpath = dirpath;
        root.volume_id = volume_id;
        root.catalog_id = catalog_id;
        root.parent_id = -1;
        root.subpaths = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Unsorted);
        IFileLoader::sortFiles(root.subpaths);
        QStringList files = dir.entryList(QDir::Files, QDir::Unsorted);
        for (const QString &f : files) {
            if (IFileLoader::isArchiveFile(f)) {
                root.subpaths << f;
            }
        }
        parentworkers << root;
    }

    int scannedCount = 0;
    int knownCount = 0;
    QList<QFuture<VolumeWorker>> workers;
    do {
        for (const VolumeWorker &p : parentworkers) {
            const QDir dir(p.dirpath);
            for (const QString &sub : p.subpaths) {
                if (isCatalogCreationCanceled()) {
                    return -1;
                }
                const QString subpath = dir.filePath(sub);
                const int sub_id = createVolumeInternal(subpath, catalog_id, p.parent_id);
                if (sub_id < 0) {
                    return -1;
                }
                // The scan runs on a worker thread, so it owns its arguments.
                const int parentVolumeId = p.volume_id;
                workers.append(QtConcurrent::run([subpath, sub_id, parentVolumeId] {
                    return createSubVolumesConcurrent(subpath, sub_id, parentVolumeId);
                }));
                ++knownCount;
            }
        }
        emit catalogProgressRangeChanged(0, knownCount);
        parentworkers.clear();

        for (const QFuture<VolumeWorker> &w : workers) {
            if (isCatalogCreationCanceled()) {
                return -1;
            }
            const VolumeWorker &v = w.result();
            emit catalogProgressValueChanged(++scannedCount);
            if (v.volume_id < 0) {
                continue;
            }
            if (v.frontPage.asc >= 0) {
                emit catalogProgressTextChanged(QFileInfo(v.dirpath).fileName());

                t_thumbs.bindValue(":width", v.frontPage.thumb.width());
                t_thumbs.bindValue(":height", v.frontPage.thumb.height());
                t_thumbs.bindValue(":thumbnail", v.frontPage.thumbbytes);
                t_thumbs.bindValue(":created_at", QDateTime::currentDateTime());
                if (!execQuery(t_thumbs, "t_thumbs")) {
                    return -1;
                }

                t_files.bindValue(":volume_id", v.volume_id);
                t_files.bindValue(":name", v.frontPage.filename);
                t_files.bindValue(":size", v.frontPage.info.size());
                t_files.bindValue(":width", v.frontPage.imagesize.width());
                t_files.bindValue(":height", v.frontPage.imagesize.height());
                t_files.bindValue(":thumb_id", t_thumbs.lastInsertId());
                //t_files.bindValue(":created_at", v.frontPage.info.created());
                t_files.bindValue(":updated_at", v.frontPage.info.lastModified());
                if (!execQuery(t_files, "t_files")) {
                    return -1;
                }

                t_fileorders.bindValue(":volume_id", v.volume_id);
                t_fileorders.bindValue(":id", t_files.lastInsertId());
                t_fileorders.bindValue(":filename_asc", v.frontPage.asc);
                if (!execQuery(t_fileorders, "t_fileorders")) {
                    return -1;
                }

                t_volumes.bindValue(":frontpage_id", t_files.lastInsertId());
                t_volumes.bindValue(":thumb_id", t_thumbs.lastInsertId());
                t_volumes.bindValue(":id", v.volume_id);
                if (!execQuery(t_volumes, "t_volumes")) {
                    return -1;
                }
            }

            if (v.subpaths.size() > 0) {
                parentworkers << v;
            }
        }
        workers.clear();
    } while (parentworkers.size() > 0);

    return volume_id;
}

static TaggedName realname2BookTitle(QString realname)
{
    // Extract book title from folder name
    // from: <<<(TAG1) [Publisher(Author)] book title (TAG2) (TAG3) ...>>>
    //   to: <<<[Publisher(Author)] book title>>>
    //
    // e.g. 'Star Wars - Han Solo (2017) (Digital) (newcomic.info)'
    //
    // from: <<<# [TAG1] [TAG2] [Publisher(Author)] book title (TAG2) [TAG4] ...>>>
    //   to: <<<[Publisher(Author)] book title>>>
    //
    // TAGs will save other fields

    TaggedName result;
    result.realname = realname;
    QList<QChar> parenthesis;
    parenthesis << '?';
    QStringList clist;
    QStringList tag;
    int cnt = 0;
    bool NumberSign = false;
    bool authorExported = false;
    int type_id = 0;
    for (QChar c : realname) {
        switch (c.unicode()) {
        case '#':
            if (cnt == 0) {
                NumberSign = true;
                parenthesis << c;
            } else if (parenthesis.last() == '#') {
                tag << c;
            } else {
                clist << c;
            }
            break;
        case '[':
            parenthesis << c;
            if (tag.size()) {
                if (tag[0] == "[") {
                    QString publisher = tag.join("");
                    result.tags << TagRecord(publisher.mid(1, publisher.length() - 2),
                                             type_id); // Normal
                } else {
                    result.tags << TagRecord(tag.join(""), type_id);
                }
                tag.clear();
            }
            type_id = NumberSign ? 0 : 2;
            tag << c;
            break;
        case ']':
            if (parenthesis.size() == 1) {
                break;
            }
            parenthesis.removeLast();
            tag << c;
            if (!NumberSign && !authorExported && tag.size()) {
                clist << tag.join("");
                QString pubauthor = tag.join("");
                result.tags << TagRecord(pubauthor.mid(1, pubauthor.length() - 2),
                                         type_id); // Publisher(Author)
                type_id = 0;
                tag.clear();
                authorExported = true;
            }
            break;
        case '(':
            if (parenthesis.last() == '[' && tag.size() >= 2) {
                QString publisher = tag.join("");
                result.tags << TagRecord(publisher.mid(1), 2); // Publisher
                type_id = 1;
                tag << c;
            } else {
                tag.clear();
                if (parenthesis.last() == '#') {
                    parenthesis.removeLast();
                    NumberSign = false;
                }
                parenthesis << c;
            }
            break;
        case ')':
            if (parenthesis.size() == 1) {
                break;
            }
            if (parenthesis.last() == '[') {
                QString author = tag.join("");
                result.tags << TagRecord(author.mid(author.indexOf('(') + 1), 3); // Author
                tag << c;
            } else {
                if (tag.size()) {
                    result.tags << TagRecord(tag.join(""), 0); // Normal
                    tag.clear();
                }
                parenthesis.removeLast();
            }
            break;
        default:
            if (parenthesis.last() == '[') {
                tag << c;
            } else {
                if (parenthesis.last() == '#') {
                    if (c != ' ') {
                        tag << c;
                    } else {
                        parenthesis.removeLast();
                    }
                } else if (NumberSign && c != ' ' && tag.size()) {
                    // last tag will be Publisher/Author
                    QString pubauthor = tag.join("");
                    result.tags << TagRecord(pubauthor.mid(1, pubauthor.length() - 2),
                                             pubauthor.indexOf("(") > 0 ? 1
                                                                        : 2); // Publisher(Author)
                    clist << tag.join("") << " " << c;
                    tag.clear();
                    NumberSign = false;
                } else if (parenthesis.last() == '(') {
                    tag << c;
                } else {
                    clist << c;
                }
            }
        }
        cnt++;
    }
    result.name = clist.join("").trimmed();
    return result;
}

int CatalogDatabase::createVolumeInternal(QString dirpath, int catalog_id, int parent_id)
{
    QFileInfo info(dirpath);
    QString realname = info.fileName();
    qDebug() << "volume: " << realname;
    emit catalogProgressTextChanged(realname);

    TaggedName tagged = realname2BookTitle(realname);

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
        t_tagentries.bindValue(":volume_id", volume_id);
        t_tagentries.bindValue(":tag_id", tag.id);
        t_tagentries.bindValue(":catalog_id", catalog_id);
        if (!execQuery(t_tagentries, "t_volumetags")) {
            return -1;
        }
    }

    return volume_id;
}

void CatalogDatabase::updateVolumeOrders()
{
    QSqlQuery t_volumeorders(m_db);
    t_volumeorders.prepare("DELETE FROM t_volumeorders");
    if (!execQuery(t_volumeorders, "t_volumeorders")) {
        return;
    }

    QSqlQuery t_volumes(m_db);
    t_volumes.prepare("SELECT id, parent_id, realname FROM t_volumes");
    if (!execQuery(t_volumes, "t_volumes")) {
        return;
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
            return;
        }
    }
}

FileWorker CatalogDatabase::createFileRecord(QString filename, QString filepath, int filename_asc)
{
    FileWorker result;
    result.filename = filename;
    result.filepath = filepath;
    result.info.setFile(filepath);
    result.asc = filename_asc;
    QImage img(filepath);
    if (!img.width()) {
        result.asc = -1;
        return result;
    }
    result.imagesize = img.size();

    QImage thumb = img.scaledToWidth(2 * ThumbnailWidth, Qt::FastTransformation);
    thumb = thumb.scaledToWidth(ThumbnailWidth, Qt::SmoothTransformation);
    QBuffer thumbdat;
    thumbdat.open(QBuffer::ReadWrite);
    if (!thumb.save(&thumbdat, "JPEG", 90)) {
        result.asc = -1;
        return result;
    }
    result.thumb = thumb;
    result.thumbbytes = thumbdat.data();
    result.created_at = QDateTime::currentDateTime();

    return result;
}

FileWorker CatalogDatabase::createFileRecordFromArchive(QString archivePath,
                                                        ImageContent &ic,
                                                        int filename_asc)
{
    FileWorker result;
    result.filename = ic.path;
    result.filepath = ic.path;
    result.info.setFile(archivePath);
    result.asc = filename_asc;
    QImage img = ic.loadedImage;
    if (!img.width()) {
        result.asc = -1;
        return result;
    }
    result.imagesize = img.size();

    QImage thumb = img.scaledToWidth(2 * ThumbnailWidth, Qt::FastTransformation);
    thumb = thumb.scaledToWidth(ThumbnailWidth, Qt::SmoothTransformation);
    QBuffer thumbdat;
    thumbdat.open(QBuffer::ReadWrite);
    if (!thumb.save(&thumbdat, "JPEG", 90)) {
        result.asc = -1;
        return result;
    }
    result.thumb = thumb;
    result.thumbbytes = thumbdat.data();
    result.created_at = QDateTime::currentDateTime();
    return result;
}

CatalogRecord CatalogDatabase::createCatalog(QString name, QString path)
{
    CatalogRecord catalog = {0};
    catalog.name = name;
    catalog.path = path;
    catalog.created_at = QDateTime::currentDateTime();

    transaction();

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
    int basevolume_id = createVolumesFrontPageOnly(path, catalog_id);
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

    if (isCatalogCreationCanceled()) {
        rollback();
        return catalog;
    }
    commit();
    catalog.created = true;
    m_volumesDirty = true;
    emit catalogCreated(catalog);

    return catalog;
}

QList<CatalogRecord> CatalogDatabase::callCreateCatalog(const QList<CatalogRecord> &newers)
{
    QList<CatalogRecord> result;
    for (const CatalogRecord &r : newers) {
        if (isCatalogCreationCanceled()) {
            break;
        }
        result << createCatalog(r.name, r.path);
        if (isCatalogCreationCanceled()) {
            break;
        }
    }
    if (result.size() > 1 || (!result.isEmpty() && result.first().created)) {
        transaction();
        updateVolumeOrders();
        commit();
    }

    // The next build starts from a clean slate, including a synchronous
    // createCatalog() call after a cancelled build.
    m_catalogCanceled.storeRelease(0);
    return result;
}

QFutureWatcher<QList<CatalogRecord>> *
CatalogDatabase::createCatalogAsync(QList<CatalogRecord> newers)
{
    m_catalogCanceled.storeRelease(0);
    QFuture<QList<CatalogRecord>> future =
        QtConcurrent::run([this, newers] { return callCreateCatalog(newers); });
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
    if (!m_volumesDirty) {
        return m_volumesCache;
    }
    QList<VolumeThumbRecord> result;
    QSqlQuery v_volumethm(m_db);
    v_volumethm.prepare("SELECT * FROM v_volumethm");
    if (!execQuery(v_volumethm, "v_volumethm")) {
        // The query failed, so the cache stays empty rather than remembering
        // the failure as the contents of the catalog.
        return result;
    }
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
        vtr.thumbnail = v_volumethm.value("thumbnail").toByteArray();
        result.append(vtr);
    }
    loadTags();
    m_volumesDirty = false;
    return m_volumesCache = result;
}

void CatalogDatabase::loadTags()
{
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

void CatalogDatabase::deleteCatalog(int id)
{
    transaction();

    QSqlQuery t_thumbs(m_db);
    t_thumbs.prepare("DELETE FROM t_thumbnails WHERE id IN (SELECT thumb_id FROM t_files WHERE "
                     "volume_id IN (SELECT id FROM t_volumes WHERE catalog_id=:catalog_id))");
    t_thumbs.bindValue(":catalog_id", id);
    if (!execQuery(t_thumbs, "t_thumbnails")) {
        rollback();
        return;
    }

    QSqlQuery t_files(m_db);
    t_files.prepare("DELETE FROM t_files WHERE volume_id IN (SELECT id FROM t_volumes WHERE "
                    "catalog_id=:catalog_id)");
    t_files.bindValue(":catalog_id", id);
    if (!execQuery(t_files, "t_files")) {
        rollback();
        return;
    }

    QSqlQuery t_fileorders(m_db);
    t_fileorders.prepare("DELETE FROM t_fileorders WHERE volume_id IN (SELECT id FROM t_volumes "
                         "WHERE catalog_id=:catalog_id)");
    t_fileorders.bindValue(":catalog_id", id);
    if (!execQuery(t_fileorders, "t_fileorders")) {
        rollback();
        return;
    }

    QSqlQuery t_volumeorders(m_db);
    t_volumeorders.prepare("DELETE FROM t_volumeorders WHERE id IN (SELECT id FROM t_volumes WHERE "
                           "catalog_id=:catalog_id)");
    t_volumeorders.bindValue(":catalog_id", id);
    if (!execQuery(t_volumeorders, "t_volumeorders")) {
        rollback();
        return;
    }

    QSqlQuery t_volumetags(m_db);
    t_volumetags.prepare("DELETE FROM t_volumetags WHERE catalog_id=:catalog_id");
    t_volumetags.bindValue(":catalog_id", id);
    if (!execQuery(t_volumetags, "t_volumetags")) {
        rollback();
        return;
    }

    QSqlQuery t_volumes(m_db);
    t_volumes.prepare("DELETE FROM t_volumes WHERE catalog_id=:catalog_id");
    t_volumes.bindValue(":catalog_id", id);
    if (!execQuery(t_volumes, "t_volumes")) {
        rollback();
        return;
    }

    QSqlQuery t_catalogs(m_db);
    t_catalogs.prepare("DELETE FROM t_catalogs WHERE id=:id");
    t_catalogs.bindValue(":id", id);
    if (!execQuery(t_catalogs, "t_catalogs")) {
        rollback();
        return;
    }

    commit();
    m_volumesDirty = true;
}

void CatalogDatabase::updateCatalogName(int id, QString name)
{
    QSqlQuery t_catalogs(m_db);
    t_catalogs.prepare("UPDATE t_catalogs SET name=:name, updated_at=:updated_at WHERE id=:id");
    t_catalogs.bindValue(":id", id);
    t_catalogs.bindValue(":name", name);
    t_catalogs.bindValue(":updated_at", QDateTime::currentDateTime());
    execQuery(t_catalogs, "t_catalogs");
}

void CatalogDatabase::deleteAllCatalogs()
{
    transaction();
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
            qDebug() << table << " delete failed: " << removal.lastError();
            rollback();
            return;
        }
    }
    commit();
    m_volumesDirty = true;
    vacuum();
}

void CatalogDatabase::transaction()
{
    if (m_transaction) {
        return;
    }
    if (!m_db.transaction()) {
        qDebug() << "m_db transaction failed: " << m_db.lastError();
        return;
    }
    m_transaction = true;
}

void CatalogDatabase::commit()
{
    if (!m_transaction) {
        return;
    }
    if (!m_db.commit()) {
        qDebug() << "m_db commit failed: " << m_db.lastError();
        return;
    }
    m_transaction = false;
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
}

void CatalogDatabase::vacuum()
{
    QSqlQuery vacuumQuery(m_db);
    if (!vacuumQuery.exec("VACUUM")) {
        qDebug() << "VACUUM failed: " << vacuumQuery.lastError();
    }
}

bool CatalogDatabase::execQuery(QSqlQuery &query, const QString &statement)
{
    if (!query.exec()) {
        qDebug() << statement << " query failed: " << query.lastError();
        return false;
    }
    return true;
}
