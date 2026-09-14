#ifndef FILELOADERSUBDIRECTORY_H
#define FILELOADERSUBDIRECTORY_H

#include "fileloaderdirectory.h"

// Source-compatible wrapper for callers that request recursive traversal.
// The behavior itself lives in FileLoaderDirectory::TraversalMode.
class FileLoaderSubDirectory : public FileLoaderDirectory
{
public:
    explicit FileLoaderSubDirectory(QString path);
};

#endif // FILELOADERSUBDIRECTORY_H
