#ifndef VOLUMEHANDLE_H
#define VOLUMEHANDLE_H

#include <memory>

class VolumeManager;

struct VolumeManagerDeleter
{
    void operator()(VolumeManager *volume) const noexcept;
};

using VolumeHandle = std::shared_ptr<VolumeManager>;

VolumeHandle makeVolumeHandle(VolumeManager *volume);

#endif // VOLUMEHANDLE_H
