#include "fileloadersubdirectory.h"

FileLoaderSubDirectory::FileLoaderSubDirectory(QObject *parent, QString path)
    : FileLoaderDirectory(parent, path, TraversalMode::Recursive)
{
}
