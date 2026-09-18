#include "imagemetadata.h"
#include "volume.h"

#include <utility>

ImageMetadata::ImageMetadata(Volume *volume, QString filename)
    : m_volume(volume),
      m_filename(std::move(filename))
{}

QDateTime ImageMetadata::getMTime() const
{
    if (m_volume->isArchive()) {
        return m_volume->fileLoader()->getFileModified(m_filename);
    }
    if (m_info.fileName().isEmpty()) {
        initFileInfo();
    }
    return m_info.lastModified();
}

qint64 ImageMetadata::getFileSize() const
{
    if (m_volume->isArchive()) {
        return m_volume->fileLoader()->getFileSize(m_filename);
    }
    if (m_info.fileName().isEmpty()) {
        initFileInfo();
    }
    return m_info.size();
}

QSize ImageMetadata::getDimension() const
{
    if (!m_dimension.isEmpty()) {
        return m_dimension;
    }
    QString aformat;
    if (IFileLoader::isExifJpegImageFile(m_filename)) {
        if (IFileLoader::supportsImageFormat(IFileLoader::turboJpegFormatName())) {
            aformat = IFileLoader::turboJpegFormatName();
        } else {
            aformat = "jpg";
        }
    } else {
        aformat = QFileInfo(m_filename.toLower()).suffix();
    }
    QByteArray bytes = m_volume->loadByteArrayByName(m_filename);
    QBuffer buffer(&bytes);
    QImageReader reader(&buffer, aformat.toUtf8());
    return m_dimension = reader.size();
}

void ImageMetadata::initFileInfo() const
{
    if (!m_volume->isArchive()) {
        m_info = QFileInfo(m_volume->pagePathForName(m_filename));
    }
}
