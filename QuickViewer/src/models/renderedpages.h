#ifndef RENDEREDPAGES_H
#define RENDEREDPAGES_H

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "renderedpage.h"
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

struct PageRenderRequest
{
    RenderedPageLayout layout;
    PageRenderSettings settings;
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
    bool add(ImageContent content, bool append, QObject *owner, QGraphicsScene *scene, const PageRenderSettings &renderSettings, bool openSeparatedPageFromEnd, QObject *resizeReceiver, std::function<void()> resizeCallback);
    void clear();

    QRect layout(const PageRenderRequest &request,
                 const EffectPreparer &prepareEffect);
    bool advanceSeparatedPage();
    bool rewindSeparatedPage();
    void setCursor(const QCursor &cursor);

    std::optional<qreal> firstDrawScale() const;
    QImage firstImage() const;
    VisiblePages contents() const;
    RenderedPageMetrics metrics() const;

private:
    RenderedPage *at(int index);
    const RenderedPage *at(int index) const;

    std::array<std::unique_ptr<RenderedPage>, Capacity> m_pages;
};

#endif // RENDEREDPAGES_H
