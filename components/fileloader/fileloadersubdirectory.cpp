#include "fileloadersubdirectory.h"

FileLoaderSubDirectory::FileLoaderSubDirectory(QString path)
    : FileLoaderDirectory(path, TraversalMode::Recursive)
{
}
