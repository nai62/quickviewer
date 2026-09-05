#ifndef VOLUMEHANDLE_H
#define VOLUMEHANDLE_H

#include <memory>

class Volume;

struct VolumeDeleter
{
    void operator()(Volume *volume) const noexcept;
};

using VolumeHandle = std::shared_ptr<Volume>;

VolumeHandle makeVolumeHandle(Volume *volume);

#endif // VOLUMEHANDLE_H
