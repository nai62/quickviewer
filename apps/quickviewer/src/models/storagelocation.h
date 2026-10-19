#ifndef STORAGELOCATION_H
#define STORAGELOCATION_H

#include <QString>

/**
 * The directory the Windows build keeps its settings, read progress and catalog
 * database in: beside the executable when the portable marker file is there,
 * and the user's roaming data directory otherwise.
 */
class StorageLocation
{
public:
    StorageLocation() = default;

    /**
     * The file whose presence beside the executable selects portable operation.
     * Its contents are not read.
     */
    static QString portableMarkerName();

    static StorageLocation resolve(const QString &applicationDirectory,
                                   const QString &userDataDirectory);

    /**
     * Sub-paths of the application's own data files, relative to the directory
     * QVApplication::getFilePathOfApplicationSetting() resolves.
     */
    static QString settingsSubPath();
    static QString readProgressSubPath();

#ifdef Q_OS_WIN
    /**
     * The directories resolve() takes, asked from Windows itself, for the
     * startup code that runs before QApplication exists.
     * nativeUserDataDirectory() is QStandardPaths::AppDataLocation of an
     * application without an organization name.
     */
    static QString nativeApplicationDirectory();
    static QString nativeUserDataDirectory(const QString &applicationName);
#endif

    bool isPortable() const { return m_portable; }
    QString directory() const { return m_directory; }

    /**
     * subPath resolved against directory(); an absolute subPath is kept.
     */
    QString filePath(const QString &subPath) const;

    /**
     * Where the catalog database is when the settings name none.
     */
    QString defaultCatalogDatabaseSubPath() const;

private:
    StorageLocation(bool portable, const QString &directory);

    bool m_portable = false;
    QString m_directory;
};

#endif // STORAGELOCATION_H
