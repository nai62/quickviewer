#ifndef PAGEDISPLAYFORMATTER_H
#define PAGEDISPLAYFORMATTER_H

#include <QtCore>

struct PageDisplayEntry
{
    QString pageName;
    QSize originalImageSize;
};

class PageDisplayFormatter
{
public:
    static QString pageNumberText(
        int firstPageIndex, int pageCount, int visiblePageCount);
    static QString statusText(
        int firstPageIndex,
        int pageCount,
        const QVector<PageDisplayEntry> &visiblePages);
    static QString signageText(
        const QString &pagePath, int pageIndex, int pageCount);
};

#endif // PAGEDISPLAYFORMATTER_H
