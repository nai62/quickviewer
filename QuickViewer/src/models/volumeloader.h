#ifndef VOLUMELOADER_H
#define VOLUMELOADER_H

#include <QtGui>
#include <QtConcurrent>
#include "volume.h"

class VolumeLoader : QObject
{
    Q_OBJECT
public:
    QString Path;
    QStringList Filenames;
    ImageContent Ic;

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
     * @brief buildForAssoc
     *
     * Special initialization method to skip the enumeration of image files
     * in the Volume and read the first image faster.
     */
    Volume *buildForAssoc();

    /**
     * @brief thumbnail
     *
     * Read and return the image of the youngest file name in Volume
     */
    ImageContent thumbnail();

    void checkBookProgress();
    /**
     * @brief A factory function that returns an instance of IFileVolume from the path of the specified file or directory
     * @return An object that inherits the IFileVolume interface. It is null if generation failed
     */
    static Volume *CreateVolume(QObject *parent, QString path);

private:
    Volume *m_volume;
    //    QFutureWatcher<void> m_watcher;
    QString m_subfilename;
};

#endif // VOLUMELOADER_H
