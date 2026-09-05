#ifndef RENDEREDPAGE_H
#define RENDEREDPAGE_H

#include <utility>

#include <QtCore>
#include <QtWidgets>

#include "imagecontent.h"

struct PageRenderSettings
{
    qreal pixelRatio = 1.0;
    RetouchParameters retouchParameters;
};

/**
 * @brief Rendered state for a page
 */
class RenderedPage : public QObject
{
    Q_OBJECT
public:
    enum PageAlign {
        PageCenter,
        PageLeft,
        PageRight
    };
    enum SeparationState {
        NotSeparated,
        FirstHalf,
        SecondHalf
    };

    explicit RenderedPage(QObject *parent = nullptr, PageRenderSettings renderSettings = {});
    RenderedPage(QObject *parent, QGraphicsScene *graphicsScene, ImageContent imageContent, PageRenderSettings renderSettings = {});
    ~RenderedPage() override;
    Q_DISABLE_COPY_MOVE(RenderedPage)

    QPoint offsetForRotation(int rotationOffset = 0) const;
    QSize rotatedImageSize(int rotationOffset = 0) const;

    /**
     * @brief Lay out an image fitted within the viewport
     */
    QRect setPageLayoutFitting(QRect viewport, PageAlign alignment, qvEnums::FitMode fitMode, qreal loupe, int rotationOffset = 0);
    QRect setPageLayoutManual(QRect viewport, PageAlign alignment, qreal scale, int rotationOffset = 0, bool loupe = false);

    void setRenderSettings(PageRenderSettings renderSettings);
    void applyResize(qreal scale, int rotationOffset, QPoint position, QSize targetSize, bool loupe = false);
    void initializePage(bool resetResizedImage = false);
    void resetSignage(QRect viewport, RenderedPage::PageAlign alignment);
    const ImageContent &imageContent() const { return m_content; }
    qreal drawScale() const { return m_drawScale; }
    qreal displayScale() const { return m_displayScale; }
    QGraphicsPixmapItem *graphicsPixmapItem() const;
    void setSignageText(QString text) { m_signageText = std::move(text); }
    void setCursor(const QCursor &cursor);
    void updateSeparationForViewport(bool separateWideImages, QSize viewportSize);
    void showLastSeparatedHalf();
    bool advanceSeparatedHalf();
    bool rewindSeparatedHalf();
signals:
    void resizeFinished();
public slots:
    void handleResizeFinished();
    void handleAnimationFrameChanged(int frameNumber);
    void handleAnimationFinished();

private:
    QImage &imageWithRetouch();
    void ensureInitialized();
    void dispose();

    QGraphicsScene *m_scene;
    ImageContent m_content;
    QGraphicsItem *m_graphicsItem;
    int m_rotationDegrees;
    QString m_signageText;
    QGraphicsTextItem *m_signageTextItem;
    QGraphicsRectItem *m_signageBackgroundItem;
    qreal m_drawScale;
    qreal m_displayScale;
    SeparationState m_separationState;
    QFutureWatcher<QImage> m_resizeWatcher;
    int m_resizeGeneratingState;
    bool m_initialized;
    PageRenderSettings m_renderSettings;
};

#endif // RENDEREDPAGE_H
