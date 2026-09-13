#include "visiblepagecomposer.h"

bool VisiblePageComposer::shouldLoadSecondPageCandidate(
    const VisiblePageCompositionRequest &request)
{
    if (request.firstPageIndex < 0 || request.firstPageIndex >= request.pageCount) {
        return false;
    }
    if (!request.options.dualViewEnabled || !request.options.secondPageAllowed) {
        return false;
    }
    if (request.firstPageIndex == 0 && request.options.firstPageAsSingle) {
        return false;
    }
    if (request.firstPageIndex + 1 >= request.pageCount) {
        return false;
    }
    return !request.options.widePageAsSingle || !request.firstPageIsLandscape;
}

VisiblePageComposition VisiblePageComposer::compose(
    const VisiblePageCompositionRequest &request)
{
    VisiblePageComposition composition;
    if (request.firstPageIndex < 0 || request.firstPageIndex >= request.pageCount) {
        return composition;
    }

    composition.pageIndexes.push_back(request.firstPageIndex);
    composition.prefetchAnchorIndex = request.firstPageIndex;
    if (shouldLoadSecondPageCandidate(request) && (!request.options.widePageAsSingle || !request.secondPageIsLandscape)) {
        composition.pageIndexes.push_back(request.firstPageIndex + 1);
        composition.prefetchAnchorIndex = request.firstPageIndex + 1;
    }
    return composition;
}
