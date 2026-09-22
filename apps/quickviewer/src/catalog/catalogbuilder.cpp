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
    if (QFileInfo(path).isFile() && IFileLoader::isArchiveFile(path)) {
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
