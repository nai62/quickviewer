#ifndef STOREDVOLUMELOCATION_H
#define STOREDVOLUMELOCATION_H

#include "volumelocation.h"

/**
 * Storage codec for volume locations kept in the application settings:
 * bookmarks and the last view path.
 *
 * Releases before the typed location API wrote an archive page as
 * "containerPath::entryName". That separator is understood only here, so the
 * rest of the application can pass VolumeLocation values around and never has
 * to look at a composite string.
 */
QString storeVolumeLocation(const VolumeLocation &location);
VolumeLocation loadStoredVolumeLocation(const QString &storedPath);

#endif // STOREDVOLUMELOCATION_H
