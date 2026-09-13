#ifndef VOLUMELOADER_H
#define VOLUMELOADER_H

#include <QtGui>
#include <QtConcurrent>
#include "volume.h"

struct VolumeBuildResult
{
    Volume *volume = nullptr;
    ArchiveOpenError error = ArchiveOpenError::None;
};

class VolumeLoader : QObject
{
    Q_OBJECT
public:
    explicit VolumeLoader(QString path);

    Volume *build();
    VolumeBuildResult buildResult();
    Volume *buildForCoverPrefetch();
    VolumeBuildResult buildForCoverPrefetchResult();

    static Volume *buildForCoverPrefetchAsync(QString path);
    Volume *buildForContainingImage();
    ImageContent loadThumbnailSourceImage();

    static Volume *createVolume(QObject *parent, QString path);
    static VolumeBuildResult createVolumeResult(QObject *parent, QString path);

private:
    VolumeBuildResult buildLoadedVolume();
    QString m_path;
    ImageContent m_initialImage;
    Volume *m_volume;
    QString m_selectedPageName;
};

#endif // VOLUMELOADER_H
