#ifndef VISIBLEPAGECOMPOSER_H
#define VISIBLEPAGECOMPOSER_H

#include <QtCore>

struct VisiblePageCompositionOptions
{
    bool dualViewEnabled = false;
    bool firstPageAsSingle = false;
    bool widePageAsSingle = false;
    bool secondPageAllowed = true;
};

struct VisiblePageCompositionRequest
{
    int firstPageIndex = -1;
    int pageCount = 0;
    bool firstPageIsLandscape = false;
    bool secondPageIsLandscape = false;
    VisiblePageCompositionOptions options;
};

struct VisiblePageComposition
{
    QVector<int> pageIndexes;
    int prefetchAnchorIndex = -1;
};

class VisiblePageComposer
{
public:
    static bool shouldLoadSecondPageCandidate(
        const VisiblePageCompositionRequest &request);
    static VisiblePageComposition compose(
        const VisiblePageCompositionRequest &request);
};

#endif // VISIBLEPAGECOMPOSER_H
