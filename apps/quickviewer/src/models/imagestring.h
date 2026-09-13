#ifndef IMAGESTRING_H
#define IMAGESTRING_H

#include <QtCore>
#include <functional>

#include "viewersession.h"
#include "renderedpagemetrics.h"

/**
 * @brief The ImageString class
 * Return outline information of the currently displayed image as text.
 *
 * These information are input as model and rendering snapshots and are
 * output as a model format character string dedicated to them.
 */
class ImageString : QObject
{
    Q_OBJECT
public:
    ImageString();
    using MetricsProvider = std::function<RenderedPageMetrics()>;

    void initialize(PageInfoProvider *pm, MetricsProvider metricsProvider);

    QString getTitleBarText();
    QString getStatusBarText();
    QString getFormatUsage();
    QString formatString(QString fmt);

private:
    PageInfoProvider *m_viewerSession;
    MetricsProvider m_metricsProvider;
};

#endif // IMAGESTRING_H
