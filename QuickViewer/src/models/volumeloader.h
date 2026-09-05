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

    Volume *build();
    Volume *buildForCoverPrefetch();

    /**
     * @brief build
     * @param onlyCover specifies to prefetch only the cover page
     *
     * Generate Volume asynchronously.
     */
    static Volume *buildForCoverPrefetchAsync(QString path);

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
    ImageContent loadThumbnailSourceImage();
    /**
     * @brief A factory function that returns an instance of IFileVolume from the path of the specified file or directory
     * @return An object that inherits the IFileVolume interface. It is null if generation failed
     */
    static Volume *createVolume(QObject *parent, QString path);

private:
    Volume *buildLoadedVolume();
    QString m_path;
    ImageContent m_initialImage;
    Volume *m_volume;
    QString m_selectedPageName;
};

#endif // VOLUMELOADER_H
