#include "renderedpages.h"

RenderedPages::~RenderedPages() = default;

int RenderedPages::count() const
{
    if (m_pages[1]) {
        return 2;
    }
    return m_pages[0] ? 1 : 0;
}

PageItem *RenderedPages::at(int index)
{
    return index >= 0 && index < count() ? m_pages[index].get() : nullptr;
}

const PageItem *RenderedPages::at(int index) const
{
    return index >= 0 && index < count() ? m_pages[index].get() : nullptr;
}

bool RenderedPages::add(ImageContent content, bool append, QObject *owner, QGraphicsScene *scene, const PageRenderSettings &renderSettings, bool openSeparatedPageFromEnd, QObject *resizeReceiver, std::function<void()> resizeCallback)
{
    const int pageCount = count();
    if (pageCount >= Capacity || !scene) {
        return false;
    }

    auto page = std::make_unique<PageItem>(
        owner, scene, std::move(content), renderSettings);
    if (openSeparatedPageFromEnd && page->separationState == PageItem::FirstHalf) {
        page->separationState = PageItem::SecondHalf;
    }
    if (resizeReceiver && resizeCallback) {
        QObject::connect(page.get(), &PageItem::resizeFinished, resizeReceiver, std::move(resizeCallback));
    }

    if (append) {
        m_pages[pageCount] = std::move(page);
    } else {
        if (pageCount == 1) {
            m_pages[1] = std::move(m_pages[0]);
        }
        m_pages[0] = std::move(page);
    }
    return true;
}

void RenderedPages::clear()
{
    m_pages[1].reset();
    m_pages[0].reset();
}

QRect RenderedPages::layout(const PageRenderRequest &request,
                            const EffectPreparer &prepareEffect)
{
    QRect sceneRect;
    const int pageCount = count();
    for (int index = 0; index < pageCount; ++index) {
        m_pages[index]->setRenderSettings(request.settings);
    }

    const RenderedPageLayout &layout = request.layout;
    for (int index = 0; index < pageCount; ++index) {
        PageItem &page = *m_pages[index];
        if (layout.separateWideImages && page.content.isLandscape()) {
            if (page.separationState == PageItem::NotSeparated && layout.viewport.width() < layout.viewport.height()) {
                page.separationState = PageItem::FirstHalf;
            }
            if (page.separationState != PageItem::NotSeparated && layout.viewport.width() > layout.viewport.height()) {
                page.separationState = PageItem::NotSeparated;
            }
        }

        PageItem::PageAlign alignment = PageItem::PageCenter;
        QRect pageRect = layout.viewport;
        if (pageCount == Capacity) {
            alignment = ((index == 0 && !layout.rightSideBook) || (index == 1 && layout.rightSideBook))
                            ? PageItem::PageLeft
                            : PageItem::PageRight;
            pageRect = QRect(
                QPoint(alignment == PageItem::PageRight
                           ? pageRect.width() / 2
                           : 0,
                       0),
                QSize(pageRect.width() / 2, pageRect.height()));
        }

        const int rotation = layout.rotations.value(index, 0);
        QRect drawRect;
        if (layout.fitMode != qvEnums::NoFitting) {
            drawRect = page.setPageLayoutFitting(
                pageRect, alignment, layout.fitMode, layout.scaleFactor, rotation);
        } else {
            drawRect = page.setPageLayoutManual(
                pageRect, alignment, layout.manualScale * layout.scaleFactor, rotation, layout.loupe);
        }
        page.signageText = layout.signage.value(index);
        page.resetSignage(layout.viewport, alignment);
        if (prepareEffect) {
            prepareEffect(dynamic_cast<QGraphicsPixmapItem *>(page.graphicsItem),
                          page.content,
                          drawRect.size());
        }
        sceneRect = sceneRect.united(drawRect);
    }
    return sceneRect;
}

bool RenderedPages::advanceSeparatedPage()
{
    PageItem *page = at(0);
    if (!page || page->separationState != PageItem::FirstHalf) {
        return false;
    }
    page->separationState = PageItem::SecondHalf;
    return true;
}

bool RenderedPages::rewindSeparatedPage()
{
    PageItem *page = at(0);
    if (!page || page->separationState != PageItem::SecondHalf) {
        return false;
    }
    page->separationState = PageItem::FirstHalf;
    return true;
}

void RenderedPages::setCursor(const QCursor &cursor)
{
    for (int index = 0; index < count(); ++index) {
        if (m_pages[index]->graphicsItem) {
            m_pages[index]->graphicsItem->setCursor(cursor);
        }
    }
}

std::optional<qreal> RenderedPages::firstDrawScale() const
{
    const PageItem *page = at(0);
    return page ? std::optional<qreal>(page->drawScale) : std::nullopt;
}

QImage RenderedPages::firstImage() const
{
    const PageItem *page = at(0);
    return page ? page->content.loadedImage : QImage();
}

VisiblePages RenderedPages::contents() const
{
    QVector<ImageContent> contents;
    contents.reserve(count());
    for (int index = 0; index < count(); ++index) {
        contents.push_back(m_pages[index]->content);
    }
    return VisiblePages(std::move(contents));
}

RenderedPageMetrics RenderedPages::metrics() const
{
    QVector<qreal> scales;
    scales.reserve(count());
    for (int index = 0; index < count(); ++index) {
        scales.push_back(m_pages[index]->displayScale);
    }
    return RenderedPageMetrics(std::move(scales));
}
