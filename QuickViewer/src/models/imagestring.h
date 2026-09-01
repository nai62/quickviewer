#ifndef IMAGESTRING_H
#define IMAGESTRING_H

#include <QtCore>
#include <functional>

#include "pagemanager.h"
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

    void initialize(PageManagerProtocol *pm, MetricsProvider metricsProvider);

    QString getTitleBarText();
    QString getStatusBarText();
    QString getFormatUsage();
    QString formatString(QString fmt);

private:
    PageManagerProtocol *m_pageManager;
    MetricsProvider m_metricsProvider;
};

#endif // IMAGESTRING_H
