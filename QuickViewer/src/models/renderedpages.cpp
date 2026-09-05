#include "renderedpages.h"

RenderedPages::~RenderedPages() = default;

int RenderedPages::count() const
{
    if (m_pages[1]) {
        return 2;
    }
    return m_pages[0] ? 1 : 0;
}

RenderedPage *RenderedPages::at(int index)
{
    return index >= 0 && index < count() ? m_pages[index].get() : nullptr;
}

const RenderedPage *RenderedPages::at(int index) const
{
    return index >= 0 && index < count() ? m_pages[index].get() : nullptr;
}

bool RenderedPages::add(ImageContent content, bool append, QObject *owner, QGraphicsScene *scene, const PageRenderSettings &renderSettings, bool openSeparatedPageFromEnd, QObject *resizeReceiver, std::function<void()> resizeCallback)
{
    const int pageCount = count();
    if (pageCount >= Capacity || !scene) {
        return false;
    }

    auto page = std::make_unique<RenderedPage>(
        owner, scene, std::move(content), renderSettings);
    if (openSeparatedPageFromEnd) {
        page->showLastSeparatedHalf();
    }
    if (resizeReceiver && resizeCallback) {
        QObject::connect(page.get(), &RenderedPage::resizeFinished, resizeReceiver, std::move(resizeCallback));
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
        RenderedPage &page = *m_pages[index];
        page.updateSeparationForViewport(
            layout.separateWideImages, layout.viewport.size());

        RenderedPage::PageAlign alignment = RenderedPage::PageCenter;
        QRect pageRect = layout.viewport;
        if (pageCount == Capacity) {
            alignment = ((index == 0 && !layout.rightSideBook) || (index == 1 && layout.rightSideBook))
                            ? RenderedPage::PageLeft
                            : RenderedPage::PageRight;
            pageRect = QRect(
                QPoint(alignment == RenderedPage::PageRight
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
        page.setSignageText(layout.signage.value(index));
        page.resetSignage(layout.viewport, alignment);
        if (prepareEffect) {
            prepareEffect(page.graphicsPixmapItem(),
                          page.imageContent(),
                          drawRect.size());
        }
        sceneRect = sceneRect.united(drawRect);
    }
    return sceneRect;
}

bool RenderedPages::advanceSeparatedPage()
{
    RenderedPage *page = at(0);
    return page && page->advanceSeparatedHalf();
}

bool RenderedPages::rewindSeparatedPage()
{
    RenderedPage *page = at(0);
    return page && page->rewindSeparatedHalf();
}

void RenderedPages::setCursor(const QCursor &cursor)
{
    for (int index = 0; index < count(); ++index) {
        m_pages[index]->setCursor(cursor);
    }
}

std::optional<qreal> RenderedPages::firstDrawScale() const
{
    const RenderedPage *page = at(0);
    return page ? std::optional<qreal>(page->drawScale()) : std::nullopt;
}

QImage RenderedPages::firstImage() const
{
    const RenderedPage *page = at(0);
    return page ? page->imageContent().loadedImage : QImage();
}

VisiblePages RenderedPages::contents() const
{
    QVector<ImageContent> contents;
    contents.reserve(count());
    for (int index = 0; index < count(); ++index) {
        contents.push_back(m_pages[index]->imageContent());
    }
    return VisiblePages(std::move(contents));
}

RenderedPageMetrics RenderedPages::metrics() const
{
    QVector<qreal> scales;
    scales.reserve(count());
    for (int index = 0; index < count(); ++index) {
        scales.push_back(m_pages[index]->displayScale());
    }
    return RenderedPageMetrics(std::move(scales));
}
