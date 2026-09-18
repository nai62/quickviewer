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
     * Classifies a real path supplied by the user or the operating system.
     */
    static OpenTarget forPath(QString path);
};

/**
 * Human readable text for messages and overlays. Returns the real file path for
 * entries inside folders and "archivePath (entryName)" for archive entries. It
 * never returns a stored form.
 */
QString volumeLocationDisplayText(const VolumeLocation &location);

Q_DECLARE_METATYPE(VolumeLocation)
Q_DECLARE_METATYPE(OpenTarget)

#endif // VOLUMELOCATION_H
