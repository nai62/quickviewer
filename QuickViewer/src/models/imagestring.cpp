#include "imagestring.h"

#include "qvapplication.h"

ImageString::ImageString()
    : m_pageManager(nullptr)
{
}

void ImageString::initialize(PageManagerProtocol *pm, MetricsProvider metricsProvider)
{
    m_pageManager = pm;
    m_metricsProvider = std::move(metricsProvider);
}

QString ImageString::getTitleBarText()
{
    if (!m_pageManager || m_pageManager->size() == 0) {
        return QString("%1 v%2").arg(qApp->applicationName()).arg(qApp->applicationVersion());
    }
    return QString("%1 - %2")
        .arg(formatString(qApp->ShowStatusBar() ? qApp->TitleTextFormat() : qApp->StatusTextFormat()))
        .arg(qApp->applicationName());
}

QString ImageString::getStatusBarText()
{
    const RenderedPageMetrics metrics = m_metricsProvider
                                            ? m_metricsProvider()
                                            : RenderedPageMetrics();
    return metrics.isEmpty()
               ? ""
               : formatString(qApp->StatusTextFormat());
}

static void addString(QStringList &tags, QString key, QString value)
{
    tags << "<tr><th>" << key << "</th><td>-</td><td>" << value << "</td></tr>";
}

QString ImageString::getFormatUsage()
{
    QStringList tags = {"<table>"};
    addString(tags, "%v", tr("Volume name (only folder/archive name), e.g. 'Sample Book')", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%V", tr("Volume full path, e.g. 'C:/Users/qv/Desktop/Sample Book'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%p", tr("Image file name (only file name), e.g. 'page01.jpg'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%P", tr("Image file path in volume, e.g. 'subpath/page01.jpg'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%Q", tr("Image file full path in volume, e.g. 'C:/Users/qv/Desktop/Sample Book/subpath/page01.jpg'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%s", tr("Image size, e.g. '1920x1080'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%m", tr("Display magnification of image, e.g. '25%'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%f", tr("Image file size in a readable format, e.g. '63.23 KB'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%F", tr("Image file size as correct number of bytes, e.g. '1,154,340 Bytes'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%b", tr("Image bitmap size with useful, e.g. '1.59 MB'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%n", tr("Current page number of the volume e.g. '33/100' or '33-34/100'", "Format tag of text displayed in title bar and status bar"));
    addString(tags, "%2", tr("Second image format separator(when 2 page spread viewing is valid)", "Format tag of text displayed in title bar and status bar"));
    tags << "</table>";
    return tags.join("");
}

QString ImageString::formatString(QString fmt)
{
    if (!m_pageManager) {
        return QString();
    }
    const VisiblePages visiblePages = m_pageManager->visiblePages();
    const RenderedPageMetrics metrics = m_metricsProvider
                                            ? m_metricsProvider()
                                            : RenderedPageMetrics();
    const int pageCount = qMin(visiblePages.count(), metrics.count());
    QList<int> pages = qApp->RightSideBook() ? QList<int>{1, 0} : QList<int>{0, 1};
    QStringList result;
    int p = 0;
    for (int i = 0; i < fmt.length(); i++) {
        QChar c = fmt.at(i);
        if (c != '%' || i + 1 == fmt.length()) {
            result << c;
            continue;
        }
        c = fmt.at(++i);
        const ImageContent fallback(
            QImage(1000, 1200, QImage::Format_RGB32),
            "page11.jpg",
            QSize(1000, 1200),
            easyexif::EXIFInfo(),
            1234567);
        const int requestedPage = pageCount == 2 && p < pages.size()
                                      ? pages[p]
                                      : 0;
        const ImageContent *page = pageCount > 0
                                       ? visiblePages.at(requestedPage)
                                       : &fallback;
        if (!page) {
            page = &fallback;
        }
        switch (c.toLatin1()) {
            // Volume name (only folder/archive name), e.g. 'Sample Book')
        case 'v': {
            QFileInfo info(m_pageManager->volumePath());
            result << info.fileName();
            break;
        }
            // Volume full path, e.g. 'C:/Users/qv/Desktop/Sample Book'
        case 'V':
            result << m_pageManager->volumePath();
            break;
            // Image file name (only file name), e.g. 'page01.jpg'
        case 'p': {
            QFileInfo info(page->path);
            result << info.fileName();
            break;
        }
            // Image file path in volume, e.g. 'subpath/page01.jpg'
        case 'P':
            result << page->path;
            break;
            // Image file full path in volume, e.g. 'C:/Users/qv/Desktop/Sample Book/subpath/page01.jpg'
        case 'Q': {
            result << m_pageManager->currentPagePath();
            break;
        }
            // Image size, e.g. '1920x1080'
        case 's':
            result << QString("%1x%2").arg(page->originalSize.width()).arg(page->originalSize.height());
            break;
            // Display magnification of image, e.g. '25%'
        case 'm':
            result << QString("%1%").arg((int)(100 * metrics.notationalScaleAt(requestedPage)));
            break;
            // Image file size in a readable format, e.g. '63.23 KB'
        case 'f': {
            double filelength = page->fileSize;
            if (filelength < 1024) {
                result << QString(tr("%1 Bytes")).arg(filelength);
            } else if (filelength < 1024 * 1024) {
                result << QString(tr("%1 KB")).arg(filelength / 1024, 0, 'f', 2);
            } else {
                result << QString(tr("%1 MB")).arg(filelength / 1024 / 1024, 0, 'f', 2);
            }
            break;
        }
            // Image file size as correct number of bytes, e.g. '1,154,340 Bytes'
        case 'F':
            result << QString(tr("%L1 Bytes")).arg(page->fileSize);
            break;
            // Image bitmap size with useful, e.g. '1.59 MB'
        case 'b': {
            double filelength = page->image.sizeInBytes();
            if (filelength < 1024) {
                result << QString(tr("%1 Bytes")).arg(filelength);
            } else if (filelength < 1024 * 1024) {
                result << QString(tr("%1 KB")).arg(filelength / 1024, 0, 'f', 2);
            } else {
                result << QString(tr("%1 MB")).arg(filelength / 1024 / 1024, 0, 'f', 2);
            }
            break;
        }
            // Current page number of the volume e.g. '33/100' or '33-34/100'
        case 'n': {
            if (pageCount == 2) {
                result << QString("%1-%2/%3").arg(m_pageManager->currentPage() + 1).arg(m_pageManager->currentPage() + 2).arg(m_pageManager->size());
            } else {
                result << QString("%1/%2").arg(m_pageManager->currentPage() + 1).arg(m_pageManager->size());
            }
            break;
        }
            // Second image format separator(when 2 page spread viewing is valid)
        case '2': {
            if (pageCount < 2) {
                i = fmt.length();
            }
            p++;
            break;
        }
        default:
            result << "%" << c;
            break;
        }
    }
    return result.join("");
}
