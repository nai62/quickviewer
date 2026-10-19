#include "storagelocation.h"

#include <QDir>
#include <QFileInfo>

StorageLocation::StorageLocation(bool portable, const QString &directory)
    : m_portable(portable),
      m_directory(directory)
{
}

QString StorageLocation::portableMarkerName()
{
    return QStringLiteral("portable.ini");
}

StorageLocation StorageLocation::resolve(const QString &applicationDirectory,
                                         const QString &userDataDirectory)
{
    if (QFileInfo(QDir(applicationDirectory).filePath(portableMarkerName())).isFile()) {
        return StorageLocation(true, applicationDirectory);
    }
    return StorageLocation(false, userDataDirectory);
}

QString StorageLocation::settingsSubPath()
{
#ifdef Q_OS_WIN
    return QStringLiteral("quickviewer.ini");
#else
    return QStringLiteral(".quickviewer/quickviewer.ini");
#endif
}

QString StorageLocation::readProgressSubPath()
{
#ifdef Q_OS_WIN
    return QStringLiteral("progress.ini");
#else
    return QStringLiteral(".quickviewer/progress.ini");
#endif
}

QString StorageLocation::filePath(const QString &subPath) const
{
    return QDir::toNativeSeparators(QDir(m_directory).filePath(subPath));
}

QString StorageLocation::defaultCatalogDatabaseSubPath() const
{
    // The portable package carries the database in its database directory.
    return m_portable ? QStringLiteral("database/thumbnail.sqlite3.db")
                      : QStringLiteral("thumbnail.sqlite3.db");
}
