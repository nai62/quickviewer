#ifndef VOLUMELOCATION_H
#define VOLUMELOCATION_H

#include <QMetaType>
#include <QString>

/**
 * Address of a container and, optionally, of an entry inside it.
 *
 * containerPath is always a real filesystem path: a folder or an archive.
 * entryName is empty when the location denotes the container itself. Otherwise
 * it names a page inside the container: a file name for folders, an archive
 * entry name for archives.
 *
 * The legacy string form "containerPath::entryName" is only accepted at the
 * boundaries that still have to read paths stored by older releases (see
 * volumeLocationFromString). It is never a filesystem path and must not reach
 * QFileInfo, QDir or user visible text.
 */
struct VolumeLocation
{
    QString containerPath;
    QString entryName;

    bool isContainer() const { return entryName.isEmpty(); }
};

/**
 * Why a location is opened. The intent tells the viewer how much it may assume
 * about the location, so callers no longer have to smuggle it through a path.
 */
enum class OpenIntent {
    Container,       // open a folder or archive as a volume
    Entry,           // open an entry the caller knows is inside the container
    FileInContainer, // open a plain image file; containerPath is its folder
};

/**
 * A location plus the reason it is being opened.
 */
struct OpenTarget
{
    VolumeLocation location;
    OpenIntent intent = OpenIntent::Container;

    static OpenTarget container(QString containerPath);
    static OpenTarget entry(VolumeLocation location);
    static OpenTarget fileInContainer(QString filePath);

    /**
     * Classifies a path supplied by the user or the operating system. Paths in
     * the legacy "containerPath::entryName" form are treated as entries so
     * that bookmarks, history and the last view path keep working.
     */
    static OpenTarget forPath(QString path);
};

/**
 * Parses the legacy "containerPath::entryName" form. Splits at the first "::"
 * and returns the whole string as the container when no separator is present.
 */
VolumeLocation volumeLocationFromString(const QString &path);

/**
 * Human readable text for messages and overlays. Returns the real file path for
 * entries inside folders and "archivePath (entryName)" for archive entries. It
 * never returns the internal "containerPath::entryName" form.
 */
QString volumeLocationDisplayText(const VolumeLocation &location);

Q_DECLARE_METATYPE(VolumeLocation)
Q_DECLARE_METATYPE(OpenTarget)

#endif // VOLUMELOCATION_H
