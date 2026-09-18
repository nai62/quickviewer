#ifndef FOLDERITEM_H
#define FOLDERITEM_H

#include <QDateTime>
#include <QString>
#include <QtGlobal>

#include <utility>

/**
 * One row of the folder view: a folder, an archive or an image, or the
 * placeholder shown when the folder has no displayable entry.
 */
class FolderItem
{
public:
    enum FileType {
        Dir,
        Archive,
        Image,
        NoItems
    };

    QString name;
    FileType type;
    QDateTime updated_at;
    qint64 size;

    FolderItem()
        : type(Dir),
          size(0)
    {}

    FolderItem(QString name, FileType type, QDateTime updated_at, qint64 size = 0)
        : name(std::move(name)),
          type(type),
          updated_at(std::move(updated_at)),
          size(size)
    {}
};

#endif // FOLDERITEM_H
