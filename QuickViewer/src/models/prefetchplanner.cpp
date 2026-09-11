#include "prefetchplanner.h"

#include <QMutableListIterator>

QList<int> PrefetchPlanner::offsets(PrefetchMode mode, int cacheCapacity)
{
    QList<int> result;
    switch (mode) {
    case PrefetchMode::Normal:
        result = {0, 1, 2, 3, -1, -2, 4, 5, -3, -4, 6, 7, -5, -6};
        while (result.size() > cacheCapacity) {
            result.removeLast();
        }
        break;
    case PrefetchMode::NormalForward:
        result = {10, 11, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7};
        if (result.size() > cacheCapacity) {
            QMutableListIterator<int> i(result);
            while (i.hasNext()) {
                if (i.next() >= cacheCapacity) {
                    i.remove();
                }
            }
        }
        break;
    case PrefetchMode::NormalBackward:
        result = {-9, -10, -7, -8, 0, 1, -1, -2, -3, -4, -5, -6};
        if (result.size() > cacheCapacity) {
            QMutableListIterator<int> i(result);
            while (i.hasNext()) {
                if (i.next() < -cacheCapacity + 2) {
                    i.remove();
                }
            }
        }
        break;
    case PrefetchMode::FastForward:
        result = {0, 1, 10, 11, -10, -9, 20, 21, -20, -19};
        while (result.size() > cacheCapacity) {
            result.removeLast();
        }
        break;
    case PrefetchMode::FastBackward:
        result = {0, 1, -10, -9, 10, 11, -20, -19, 20, 21};
        while (result.size() > cacheCapacity) {
            result.removeLast();
        }
        break;
    default:
        break;
    }
    return result;
}

QList<int> PrefetchPlanner::indexes(PrefetchMode mode, int currentIndex, int pageCount, int cacheCapacity)
{
    QList<int> result;
    const QList<int> plannedOffsets = offsets(mode, cacheCapacity);
    for (const int offset : plannedOffsets) {
        const int index = currentIndex + offset;
        if (index >= 0 && index < pageCount) {
            result.append(index);
        }
    }
    return result;
}
