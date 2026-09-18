#include "storedvolumelocation.h"

#include <QDir>
#include <QString>

#include "fileloader.h"

namespace {

// Legacy separator between the container path and the entry inside it. It is
// spelled out here and nowhere else.
const QString &separator()
{
    static const QString separator = QStringLiteral("::");
    return separator;
}

} // namespace

QString storeVolumeLocation(const VolumeLocation &location)
{
    const QString containerPath = QDir::fromNativeSeparators(location.containerPath);
    if (location.isContainer()) {
        return containerPath;
    }
    if (!IFileLoader::isArchiveFile(containerPath)) {
        // A folder page is a real file, and that is how it has always been
        // stored.
        return QDir::fromNativeSeparators(QDir(containerPath).absoluteFilePath(location.entryName));
    }
    return containerPath + separator() + location.entryName;
}

VolumeLocation loadStoredVolumeLocation(const QString &storedPath)
{
    const int separatorIndex = storedPath.indexOf(separator());
    if (separatorIndex < 0) {
        return {QDir::fromNativeSeparators(storedPath), QString()};
    }
    return {QDir::fromNativeSeparators(storedPath.left(separatorIndex)),
            storedPath.mid(separatorIndex + separator().size())};
}
