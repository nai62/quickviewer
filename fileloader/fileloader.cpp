#include "fileloader.h"

#include <algorithm>

#include <QImageReader>

#ifdef Q_OS_WIN
#    include <Shlwapi.h>
#endif

namespace {

QString fileSuffix(const QString &path)
{
    return QFileInfo(path).suffix().toLower();
}

const QList<QByteArray> &supportedImageFormats()
{
    static const QList<QByteArray> formats = [] {
        QList<QByteArray> result = QImageReader::supportedImageFormats();
        const QList<QByteArray> extraFormats{
            "jpe",
            "jif",
            "jfif",
            "jfi",
            "heic",
            "heif",
        };
        for (const QByteArray &format : extraFormats) {
            if (!result.contains(format)) {
                result.append(format);
            }
        }
        return result;
    }();
    return formats;
}

const QStringList &exifJpegImageFormats()
{
    static const QStringList formats{"jpg", "jpeg", "jpe"};
    return formats;
}

const QStringList &exifRawImageFormats()
{
    static const QStringList formats{"crw", "cr2", "arw", "nef", "raf", "dng", "tif", "tiff"};
    return formats;
}

const QStringList &animatedImageFormats()
{
    static const QStringList formats{"gif", "apng"};
    return formats;
}

const QStringList &archiveFormats()
{
    static const QStringList formats{"zip", "7z", "rar", "cbr", "cbz"};
    return formats;
}

} // namespace

bool IFileLoader::isImageFile(QString path)
{
    return supportedImageFormats().contains(fileSuffix(path).toLatin1());
}

bool IFileLoader::supportsImageFormat(const QByteArray &format)
{
    return QImageReader::supportedImageFormats().contains(format.toLower());
}

bool IFileLoader::isArchiveFile(QString path)
{
    return archiveFormats().contains(fileSuffix(path));
}

bool IFileLoader::isExifJpegImageFile(QString path)
{
    return exifJpegImageFormats().contains(fileSuffix(path));
}

bool IFileLoader::isExifRawImageFile(QString path)
{
    return exifRawImageFormats().contains(fileSuffix(path));
}

bool IFileLoader::isAnimatedImageFile(QString path)
{
    return animatedImageFormats().contains(fileSuffix(path));
}

void IFileLoader::sortFiles(QStringList &filenames)
{
    std::sort(filenames.begin(), filenames.end(), caseInsensitiveLessThan);
}

#ifdef Q_OS_WIN
bool IFileLoader::caseInsensitiveLessThan(const QString &s1, const QString &s2)
{
    const std::wstring ss1(s1.toStdWString());
    const std::wstring ss2(s2.toStdWString());
    return ::StrCmpLogicalW(ss1.c_str(), ss2.c_str()) < 0;
}
#else
bool IFileLoader::caseInsensitiveLessThan(const QString &s1, const QString &s2)
{
    return s1.toLower() < s2.toLower();
}
#endif

quint64 IFileLoader::getFileSize(QString filename) const
{
    Q_UNUSED(filename);
    return 0;
}

QDateTime IFileLoader::getFileModified(QString filename) const
{
    Q_UNUSED(filename);
    return QDateTime();
}
