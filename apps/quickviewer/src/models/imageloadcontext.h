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
        QMutexLocker locker(&m_mutex);
        return m_loader ? m_loader->getFileResult(name) : FileLoadResult{};
    }

    QByteArray load(const QString &name) { return loadResult(name).data; }

    IFileLoader *loader() const { return m_loader.get(); }

private:
    std::unique_ptr<IFileLoader> m_loader;
    QMutex m_mutex;
};

#endif // IMAGELOADCONTEXT_H
