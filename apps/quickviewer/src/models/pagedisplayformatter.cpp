#include "pagedisplayformatter.h"

QString PageDisplayFormatter::pageNumberText(
    int firstPageIndex, int pageCount, int visiblePageCount)
{
    if (firstPageIndex < 0 || firstPageIndex >= pageCount || visiblePageCount <= 0) {
        return {};
    }
    if (visiblePageCount == 2) {
        return QString("(%1-%2/%3)")
            .arg(firstPageIndex + 1)
            .arg(firstPageIndex + 2)
            .arg(pageCount);
    }
    return QString("(%1/%2)").arg(firstPageIndex + 1).arg(pageCount);
}

QString PageDisplayFormatter::statusText(
    int firstPageIndex,
    int pageCount,
    const QVector<PageDisplayEntry> &visiblePages)
{
    const QString pageNumbers =
        pageNumberText(firstPageIndex, pageCount, visiblePages.size());
    switch (visiblePages.size()) {
    case 1:
        return QString("%1 %2[%3x%4]")
            .arg(visiblePages[0].pageName)
            .arg(pageNumbers)
            .arg(visiblePages[0].originalImageSize.width())
            .arg(visiblePages[0].originalImageSize.height());
    case 2:
        return QString("%1 %2[%3x%4] | %5 [%6x%7]")
            .arg(visiblePages[0].pageName)
            .arg(pageNumbers)
            .arg(visiblePages[0].originalImageSize.width())
            .arg(visiblePages[0].originalImageSize.height())
            .arg(visiblePages[1].pageName)
            .arg(visiblePages[1].originalImageSize.width())
            .arg(visiblePages[1].originalImageSize.height());
    default:
        return {};
    }
}

QString PageDisplayFormatter::signageText(
    const QString &pagePath, int pageIndex, int pageCount)
{
    if (pagePath.isEmpty() || pageIndex < 0 || pageIndex >= pageCount) {
        return {};
    }
    return QString("%1 (%2/%3)")
        .arg(pagePath)
        .arg(pageIndex + 1)
        .arg(pageCount);
}
