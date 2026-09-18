#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QtCore>

class Volume;

/**
 * Attributes of one page of a volume, such as its file name, size and
 * modification time, resolved lazily when they are asked for. Instances are
 * copied into the volume's metadata list, so they are plain values.
 */
class ImageMetadata
{
public:
    ImageMetadata(Volume *volume, QString filename);

    QString filename() const { return m_filename; }
    QDateTime getMTime() const;
    qint64 getFileSize() const;
    QSize getDimension() const;

private:
    void initFileInfo() const;
    Volume *m_volume;
    QString m_filename;
    mutable QFileInfo m_info;
    mutable QSize m_dimension;
};

#endif // IMAGEMETADATA_H
