#ifndef IMAGELOADCONTEXT_H
#define IMAGELOADCONTEXT_H

#include <QMutex>
#include <QMutexLocker>
#include <QString>

#include <memory>
#include <utility>

#include "fileloader.h"

class ImageLoadContext
{
public:
    explicit ImageLoadContext(std::unique_ptr<IFileLoader> loader)
        : m_loader(std::move(loader))
    {
    }

    FileLoadResult loadResult(const QString &name)
    {
        if (!m_loader) {
            return {};
        }
        // A folder read waiting for the lock would wait for every other page
        // being read, the one the viewer needs included.
        if (m_loader->supportsConcurrentReads()) {
            return m_loader->getFileResult(name);
        }
        QMutexLocker locker(&m_mutex);
        return m_loader->getFileResult(name);
    }

    QByteArray load(const QString &name) { return loadResult(name).data; }

    IFileLoader *loader() const { return m_loader.get(); }

private:
    std::unique_ptr<IFileLoader> m_loader;
    QMutex m_mutex;
};

#endif // IMAGELOADCONTEXT_H
