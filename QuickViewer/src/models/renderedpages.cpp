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

bool RenderedPages::add(ImageContent content, bool append, QObject *owner, QGraphicsScene *scene, const PageRenderContext *renderContext, bool openSeparatedPageFromEnd, QObject *resizeReceiver, std::function<void()> resizeCallback)
{
    const int pageCount = count();
    if (pageCount >= Capacity || !scene) {
        return false;
    }

    auto page = std::make_unique<PageItem>(
        owner, scene, std::move(content), renderContext);
    if (openSeparatedPageFromEnd && page->Separation == PageItem::FirstSeparated) {
        page->Separation = PageItem::SecondSeparated;
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

QRect RenderedPages::layout(const RenderedPageLayout &layout,
                            const EffectPreparer &prepareEffect)
{
    QRect sceneRect;
    const int pageCount = count();
    for (int index = 0; index < pageCount; ++index) {
        PageItem &page = *m_pages[index];
        if (layout.separateWideImages && page.Ic.isLandscape()) {
            if (page.Separation == PageItem::NoSeparated && layout.viewport.width() < layout.viewport.height()) {
                page.Separation = PageItem::FirstSeparated;
            }
            if (page.Separation != PageItem::NoSeparated && layout.viewport.width() > layout.viewport.height()) {
                page.Separation = PageItem::NoSeparated;
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
        page.Text = layout.signage.value(index);
        page.resetSignage(layout.viewport, alignment);
        if (prepareEffect) {
            prepareEffect(dynamic_cast<QGraphicsPixmapItem *>(page.GrItem),
                          page.Ic,
                          drawRect.size());
        }
        sceneRect = sceneRect.united(drawRect);
    }
    return sceneRect;
}

bool RenderedPages::advanceSeparatedPage()
{
    PageItem *page = at(0);
    if (!page || page->Separation != PageItem::FirstSeparated) {
        return false;
    }
    page->Separation = PageItem::SecondSeparated;
    return true;
}

bool RenderedPages::rewindSeparatedPage()
{
    PageItem *page = at(0);
    if (!page || page->Separation != PageItem::SecondSeparated) {
        return false;
    }
    page->Separation = PageItem::FirstSeparated;
    return true;
}

void RenderedPages::setCursor(const QCursor &cursor)
{
    for (int index = 0; index < count(); ++index) {
        if (m_pages[index]->GrItem) {
            m_pages[index]->GrItem->setCursor(cursor);
        }
    }
}

std::optional<qreal> RenderedPages::firstDrawScale() const
{
    const PageItem *page = at(0);
    return page ? std::optional<qreal>(page->DrawScale) : std::nullopt;
}

QImage RenderedPages::firstImage() const
{
    const PageItem *page = at(0);
    return page ? page->Ic.image : QImage();
}

VisiblePages RenderedPages::contents() const
{
    QVector<ImageContent> contents;
    contents.reserve(count());
    for (int index = 0; index < count(); ++index) {
        contents.push_back(m_pages[index]->Ic);
    }
    return VisiblePages(std::move(contents));
}

RenderedPageMetrics RenderedPages::metrics() const
{
    QVector<qreal> scales;
    scales.reserve(count());
    for (int index = 0; index < count(); ++index) {
        scales.push_back(m_pages[index]->NotationalScale);
    }
    return RenderedPageMetrics(std::move(scales));
}
