#ifndef CATALOGBUILDER_H
#define CATALOGBUILDER_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

#include "catalogrecords.h"

/**
 * The front page of one volume, prepared for storage in the catalog database.
 * An empty cover means the catalog stores the volume without a thumbnail.
 */
class CatalogCover
{
public:
    QString name;         // file name of the cover image inside the volume
    QSize imageSize;      // pixel size of the source image
    QSize thumbnailSize;  // pixel size of the stored JPEG thumbnail
    QByteArray thumbnail; // JPEG bytes for t_thumbnails
    qint64 size;          // byte size of the source file
    QDateTime updated;    // last modification time of the source file
    bool isEmpty() const { return thumbnail.isEmpty(); }
};

/** What one folder of a catalog holds. */
class CatalogFolderScan
{
public:
    QStringList subVolumeNames;
    CatalogCover cover;
};

/**
 * Reads catalog content from the file system. The builder never touches the
 * catalog database, so a scan can run on a worker thread.
 */
class CatalogBuilder
{
public:
    /**
     * Scans one folder of a catalog.
     *
     * \a baseFolder marks the folder the catalog was created from. That folder
     * is the catalog's base volume and lists the archives next to its
     * subfolders as volumes. Every folder takes its front page from its
     * first image; the folders below the base contribute only subfolders.
     */
    static CatalogFolderScan scanFolder(const QString &path, bool baseFolder);
};

#endif // CATALOGBUILDER_H
