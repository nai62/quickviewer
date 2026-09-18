#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QtCore>

class Volume;

/**
 * @brief The ImageMetadata class
 *
 * This class holds various attributes of image files, such as file names and file sizes,
 * and provides a function to retrieve them when necessary.
 */
class ImageMetadata : QObject
{
    Q_OBJECT
public:
    ImageMetadata(Volume *volume, QString filename);
    ImageMetadata(const ImageMetadata &rhs)
    {
        *this = rhs;
    }
    ImageMetadata &operator=(const ImageMetadata &rhs)
    {
        m_volume = rhs.m_volume;
        m_filename = rhs.m_filename;
        m_info = rhs.m_info;
        m_dimension = rhs.m_dimension;
        return *this;
    }

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
