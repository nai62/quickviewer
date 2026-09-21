#include "catalogbuilder.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImage>

#include "fileloader.h"
#include "imagecontent.h"
#include "volume.h"
#include "volumeloader.h"

namespace {

// Width of the thumbnails stored in the catalog database.
constexpr int ThumbnailWidth = 96;
// JPEG quality of the stored thumbnails.
constexpr int ThumbnailJpegQuality = 90;

/** Thumbnail of \a image the way the catalog stores it. */
CatalogCover coverFromImage(const QImage &image, const QString &name, const QFileInfo &info)
{
    CatalogCover cover;
    if (image.isNull()) {
        return cover;
    }
    cover.name = name;
    cover.imageSize = image.size();
    cover.size = info.size();
    cover.updated = info.lastModified();

    QImage thumbnail = image.scaledToWidth(2 * ThumbnailWidth, Qt::FastTransformation);
    thumbnail = thumbnail.scaledToWidth(ThumbnailWidth, Qt::SmoothTransformation);
    QBuffer bytes;
    bytes.open(QBuffer::ReadWrite);
    if (!thumbnail.save(&bytes, "JPEG", ThumbnailJpegQuality)) {
        return CatalogCover();
    }
    cover.thumbnailSize = thumbnail.size();
    cover.thumbnail = bytes.data();
    return cover;
}

/**
 * Front page of the folder \a dir: the first image in display order. Only that
 * one image is tried, so a front page that cannot be read leaves the volume
 * without a cover instead of stepping to the next image.
 */
CatalogCover frontPageOfFolder(const QDir &dir)
{
    QStringList files = dir.entryList(QDir::Files, QDir::Unsorted);
    IFileLoader::sortFiles(files);
    for (const QString &file : files) {
        if (!IFileLoader::isImageFile(file)) {
            continue;
        }
        const QString path = dir.filePath(file);
        return coverFromImage(QImage(path), file, QFileInfo(path));
    }
    return CatalogCover();
}

} // namespace

CatalogFolderScan CatalogBuilder::scanFolder(const QString &path, bool baseFolder)
{
    CatalogFolderScan scan;
    if (IFileLoader::isArchiveFile(path)) {
        // An archive is a leaf volume whose front page is the first page inside.
        VolumeLoader volumeLoader(path);
        const ImageContent content = volumeLoader.loadThumbnailSourceImage();
        if (!content.loadedImage.isNull()) {
            scan.cover = coverFromImage(content.loadedImage, content.path, QFileInfo(path));
        }
        return scan;
    }

    const QDir dir(path);
    QStringList subVolumeNames = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Unsorted);
    IFileLoader::sortFiles(subVolumeNames);
    if (baseFolder) {
        const QStringList files = dir.entryList(QDir::Files, QDir::Unsorted);
        for (const QString &file : files) {
            if (IFileLoader::isArchiveFile(file)) {
                subVolumeNames << file;
            }
        }
    }
    scan.subVolumeNames = subVolumeNames;
    // A folder that holds images is a volume of its own, whether it is the
    // folder the catalog was created from or a folder below it.
    scan.cover = frontPageOfFolder(dir);
    return scan;
}

TaggedName CatalogBuilder::parseVolumeName(const QString &realname)
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
