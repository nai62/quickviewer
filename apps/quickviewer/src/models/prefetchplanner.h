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
    InitialDisplay,
};

Q_DECLARE_METATYPE(PrefetchMode)

/** How far the prefetch takes a planned page. */
enum class PrefetchDepth {
    /** Decoded, ready to show. */
    Decode,
    /** Read - for an archive, extracted - but left encoded. */
    Read,
};

struct PrefetchTarget
{
    int index = -1;
    PrefetchDepth depth = PrefetchDepth::Read;

    bool operator==(const PrefetchTarget &other) const
    {
        return index == other.index && depth == other.depth;
    }
};

class PrefetchPlanner
{
public:
    /** Pages this far from the current one, or closer, are always decoded. */
    static constexpr int DecodedRadius = 3;

    static QList<int> offsets(PrefetchMode mode, int cacheCapacity);
    static QList<int>
    indexes(PrefetchMode mode, int currentIndex, int pageCount, int cacheCapacity);
    /**
     * The pages indexes() plans, each with how far to take it: the pages
     * around the current one are decoded, and so are the two a fast turn in
     * the same direction lands on. The others are only read.
     */
    static QList<PrefetchTarget>
    targets(PrefetchMode mode, int currentIndex, int pageCount, int cacheCapacity);
    static PrefetchDepth depth(PrefetchMode mode, int offset);
};

#endif // PREFETCHPLANNER_H
