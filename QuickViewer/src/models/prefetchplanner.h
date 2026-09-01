#ifndef PREFETCHPLANNER_H
#define PREFETCHPLANNER_H

#include <QList>
#include <QMetaType>

enum class PrefetchMode {
    Normal,
    NormalForward,
    NormalBackward,
    FastForward,
    FastBackward,
    CoverOnly,
    CreateThumbnail,
};

Q_DECLARE_METATYPE(PrefetchMode)

class PrefetchPlanner
{
public:
    static QList<int> offsets(PrefetchMode mode, int cacheCapacity);
    static QList<int> indexes(PrefetchMode mode, int currentIndex, int pageCount, int cacheCapacity);
};

#endif // PREFETCHPLANNER_H
