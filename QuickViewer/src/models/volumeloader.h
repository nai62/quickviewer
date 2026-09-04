#ifndef VOLUMELOADER_H
#define VOLUMELOADER_H

#include <QtGui>
#include <QtConcurrent>
#include "volume.h"

class VolumeLoader : QObject
{
    Q_OBJECT
public:
    explicit VolumeLoader(QString path);

    /**
     * @brief build
     * @param onlyCover specifies to prefetch only the cover page
     *
     * Generate Volume synchronously.
     */
    Volume *build(bool onlyCover);

    /**
     * @brief build
     * @param onlyCover specifies to prefetch only the cover page
     *
     * Generate Volume asynchronously.
     */
    static Volume *buildAsync(QString path, bool onlyCover);

    /**
     * @brief buildForContainingImage
     *
     * Special initialization method to skip the enumeration of image files
     * in the Volume and read the first image faster.
     */
    Volume *buildForContainingImage();

    /**
     * @brief thumbnail
     *
     * Read and return the image of the youngest file name in Volume
     */
    ImageContent thumbnail();

    void restoreReadProgress();
    /**
     * @brief A factory function that returns an instance of IFileVolume from the path of the specified file or directory
     * @return An object that inherits the IFileVolume interface. It is null if generation failed
     */
    static Volume *createVolume(QObject *parent, QString path);

private:
    QString m_path;
    QStringList m_pageNames;
    ImageContent m_initialImage;
    Volume *m_volume;
    //    QFutureWatcher<void> m_watcher;
    QString m_selectedPageName;
};

#endif // VOLUMELOADER_H
