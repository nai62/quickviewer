#ifndef IMAGELOADCONTEXT_H
#define IMAGELOADCONTEXT_H

#include <QMutex>
#include <QString>

#include "fileloader.h"

class ImageLoadContext
{
public:
    explicit ImageLoadContext(IFileLoader *loader)
        : m_loader(loader)
    {
    }

    ~ImageLoadContext()
    {
        if (m_loader) {
            m_loader->deleteLater();
        }
    }

    QByteArray load(const QString &name)
    {
        return m_loader->getFile(name, m_mutex);
    }

    IFileLoader *loader() const { return m_loader; }

private:
    IFileLoader *m_loader;
    QMutex m_mutex;
};

#endif // IMAGELOADCONTEXT_H
