#include "storagelocation.h"

#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#    include <string>

#    include <shlobj.h>
// After shlobj.h, which brings in the base definitions knownfolders.h needs.
#    include <knownfolders.h>
#endif

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

#ifdef Q_OS_WIN
QString StorageLocation::nativeApplicationDirectory()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length =
            ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return QString();
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        // A path that fills the buffer may have been cut short.
        buffer.resize(buffer.size() * 2);
    }
    return QFileInfo(QString::fromStdWString(buffer)).absolutePath();
}

QString StorageLocation::nativeUserDataDirectory(const QString &applicationName)
{
    PWSTR path = nullptr;
    QString directory;
    if (SUCCEEDED(
            ::SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DONT_VERIFY, nullptr, &path))) {
        directory = QDir::fromNativeSeparators(QString::fromWCharArray(path)) + QLatin1Char('/') +
                    applicationName;
    }
    ::CoTaskMemFree(path);
    return directory;
}
#endif

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
