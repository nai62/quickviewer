#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QtCore>

/**
 * Showing a path where the platform lists files: Explorer on Windows, and
 * whatever handles a folder elsewhere.
 */

/**
 * The argument Explorer is given to show \a path: a folder to open, or an entry
 * to show selected inside the folder that holds it, both quoted. Empty when the
 * path is empty or names nothing that exists, because Explorer opens somewhere
 * else entirely for a path it cannot resolve.
 */
QString explorerArgument(const QString &path);

/** Shows \a path in the file manager, selecting the entry where it can. */
void showInFileManager(const QString &path);

#endif // FILEMANAGER_H
