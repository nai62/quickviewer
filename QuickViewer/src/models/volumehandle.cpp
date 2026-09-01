#include "volumehandle.h"

#include <QMetaObject>
#include <QThread>

#include "volumemanager.h"

void VolumeManagerDeleter::operator()(VolumeManager *volume) const noexcept
{
    if(!volume)
        return;

    QThread *ownerThread = volume->thread();
    if(!ownerThread || ownerThread == QThread::currentThread()) {
        delete volume;
        return;
    }

    // QObject destruction must happen in the thread that owns the volume and
    // its loader. The context object also cancels this callback if an external
    // owner has already destroyed the volume.
    if(!QMetaObject::invokeMethod(volume, [volume] {
        delete volume;
    }, Qt::QueuedConnection)) {
        qWarning("Could not queue VolumeManager destruction on its owner thread");
    }
}

VolumeHandle makeVolumeHandle(VolumeManager *volume)
{
    if(!volume)
        return {};
    return VolumeHandle(volume, VolumeManagerDeleter());
}
