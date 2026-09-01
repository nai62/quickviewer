#ifndef RENDEREDPAGES_H
#define RENDEREDPAGES_H

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "pagecontent.h"
#include "renderedpagemetrics.h"
#include "visiblepages.h"

struct RenderedPageLayout
{
    QRect viewport;
    qvEnums::FitMode fitMode = qvEnums::NoFitting;
    qreal manualScale = 1.0;
    qreal scaleFactor = 1.0;
    bool loupe = false;
    bool separateWideImages = false;
    bool rightSideBook = false;
    QVector<int> rotations;
    QStringList signage;
};

class RenderedPages
{
public:
    static constexpr int Capacity = 2;
    using EffectPreparer = std::function<void(
        QGraphicsPixmapItem *, const ImageContent &, QSize)>;

    RenderedPages() = default;
    ~RenderedPages();
    Q_DISABLE_COPY_MOVE(RenderedPages)

    int count() const;
    bool add(ImageContent content, bool append, QObject *owner, QGraphicsScene *scene, const PageRenderContext *renderContext, bool openSeparatedPageFromEnd, QObject *resizeReceiver, std::function<void()> resizeCallback);
    void clear();

    QRect layout(const RenderedPageLayout &layout,
                 const EffectPreparer &prepareEffect);
    bool advanceSeparatedPage();
    bool rewindSeparatedPage();
    void setCursor(const QCursor &cursor);

    std::optional<qreal> firstDrawScale() const;
    QImage firstImage() const;
    VisiblePages contents() const;
    RenderedPageMetrics metrics() const;

private:
    PageItem *at(int index);
    const PageItem *at(int index) const;

    std::array<std::unique_ptr<PageItem>, Capacity> m_pages;
};

#endif // RENDEREDPAGES_H
