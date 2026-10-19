#include "textimagecache.h"
#include "startupprofiler.h"
#include <cstdio>
#ifdef Q_OS_WIN
#    include <windows.h>
#    include <fcntl.h>
#    include <io.h>
#endif

namespace {
constexpr quint32 MaxPacket = 32 * 1024 * 1024;
constexpr int IdleShutdownMilliseconds = 10000;
/** Widest text a caption has the helper wrap, and the tallest block it lays out. */
constexpr int MaxWrapWidth = 4096;
constexpr int MaxWrapHeight = 4096;
/**
 * Most pixels one wrapped mask may cover. The parent refuses a packet it cannot
 * carry and fails every request behind it, so an oversized one is refused here.
 */
constexpr qsizetype MaxImagePixels = 6 * 1024 * 1024;
QByteArray packet(const QByteArray &payload)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream << quint32(payload.size());
    result += payload;
    return result;
}

constexpr int WrapFlags = Qt::TextWordWrap | Qt::TextWrapAnywhere;

/**
 * The lines an item view shows of \a text in the box \a layout gives it: the
 * lines that fit, and the last of them elided when more of the text follows,
 * the way a view lays out a wrapped item.
 */
QString boxLines(const QString &text,
                 const QFont &font,
                 QPaintDevice *device,
                 const TextImageLayout &layout)
{
    const QFontMetrics metrics(font, device);
    QTextLayout textLayout(text, font, device);
    QTextOption option;
    option.setWrapMode((layout.flags & Qt::TextWrapAnywhere)
                           ? QTextOption::WrapAtWordBoundaryOrAnywhere
                           : QTextOption::WordWrap);
    textLayout.setTextOption(option);
    textLayout.beginLayout();
    QStringList lines;
    int height = 0;
    for (QTextLine line = textLayout.createLine(); line.isValid(); line = textLayout.createLine()) {
        line.setLineWidth(layout.width);
        QString lineText = text.mid(line.textStart(), line.textLength());
        const bool more = line.textStart() + line.textLength() < text.size();
        if (more && height + 2 * metrics.lineSpacing() > layout.height) {
            // The last line that fits stands for the rest of the text.
            if (layout.elideMode != Qt::ElideNone) {
                lineText =
                    metrics.elidedText(text.mid(line.textStart()), layout.elideMode, layout.width);
            }
            lines.append(lineText);
            break;
        }
        while (!lineText.isEmpty() && lineText.back().isSpace()) {
            lineText.chop(1);
        }
        lines.append(lineText);
        height += metrics.lineSpacing();
    }
    textLayout.endLayout();
    return lines.join(QLatin1Char('\n'));
}

TextImageResult render(const QByteArray &key)
{
    QString text;
    QFont font;
    qreal ratio = 1;
    TextImageLayout layout;
    int elideMode = Qt::ElideRight;
    QDataStream input(key);
    input >> text >> font >> ratio >> layout.width >> layout.height >> layout.flags >> elideMode;
    layout.elideMode = Qt::TextElideMode(elideMode);
    if (input.status() != QDataStream::Ok || !qIsFinite(ratio) || ratio < 0.5 || ratio > 8 ||
        text.size() > 32768 || layout.width < 0 || layout.width > MaxWrapWidth ||
        layout.height < 0 || layout.height > MaxWrapHeight || elideMode < Qt::ElideLeft ||
        elideMode > Qt::ElideNone) {
        return {};
    }
    const bool wrapped = layout.width > 0 && (layout.flags & WrapFlags);
    const bool boxed = wrapped && layout.height > 0;
    const int alignment = layout.flags & ~WrapFlags;
    auto result = QSharedPointer<TextImages>::create();
    for (int bold = 0; bold < TextImages::ImageCount; ++bold) {
        if (bold) {
            font.setBold(true);
        }
        // A small logical paint device transfers the actual view's DPI. Font
        // point sizes must not depend on the helper's default screen.
        QImage device(1, 1, QImage::Format_ARGB32_Premultiplied);
        device.setDevicePixelRatio(ratio);
        QFontMetrics metrics(font, &device);
        // A label wraps the text the way the label does, so the image covers
        // the lines the label draws; an item view keeps what fits in its box;
        // anything else gets one line, elided to the width it is given or to
        // what a list row can hold.
        QString visible;
        QSize content;
        QRect lines;
        if (boxed) {
            visible = boxLines(text, font, &device, layout);
            content = QSize(layout.width, layout.height);
        } else if (wrapped) {
            lines = metrics.boundingRect(
                QRect(0, 0, layout.width, MaxWrapHeight), alignment | Qt::TextWordWrap, text);
            content = lines.size();
        } else {
            visible = layout.width > 0 ? metrics.elidedText(text, layout.elideMode, layout.width)
                                       : metrics.elidedText(text, Qt::ElideRight, 4096);
            content = QSize(metrics.horizontalAdvance(visible), metrics.height());
        }
        const int width = qBound(1, content.width() + 4, 8192);
        const int height = qBound(1, content.height() + 4, 16384);
        const QSize imageSize(qCeil(width * ratio), qCeil(height * ratio));
        if (wrapped && qsizetype(imageSize.width()) * imageSize.height() > MaxImagePixels) {
            return {};
        }
        QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(ratio);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setFont(font);
        // Coverage only: the GUI tints the mask with the colour it draws in.
        painter.setPen(Qt::white);
        const QRect area(QPoint(2, 2), content);
        if (boxed) {
            painter.setClipRect(area);
            painter.drawText(area, alignment, visible);
        } else if (wrapped) {
            // Wrapped at the width the label has, and moved so that the lines
            // start at the image's margin whichever side they align to.
            painter.drawText(QRect(2 - lines.x(), 2, layout.width, lines.height()),
                             alignment | Qt::TextWordWrap,
                             text);
        } else {
            painter.drawText(QPoint(2, 2 + metrics.ascent()), visible);
        }
        painter.end();
        result->images.append(image);
    }
    return result;
}
}

QImage tintedTextMask(const QImage &mask, const QColor &color)
{
    static QCache<QPair<qint64, QRgb>, QImage> cache(2 * 1024); // KiB
    const QPair<qint64, QRgb> key(mask.cacheKey(), color.rgba());
    if (const QImage *cached = cache.object(key)) {
        return *cached;
    }
    QImage image(mask.size(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(mask.devicePixelRatio());
    image.fill(color);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.drawImage(QPoint(0, 0), mask);
    painter.end();
    cache.insert(key, new QImage(image), qMax(1, int(image.sizeInBytes() / 1024)));
    return image;
}

TextImageCache::TextImageCache(QObject *parent)
    : QObject(parent),
      m_results(32 * 1024) // KiB; models retain their visible results
{
#ifdef Q_OS_WIN
    // A background GUI-subsystem child must not activate Windows startup
    // feedback (the busy cursor), even though it never shows a window.
    m_process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->startupInfo->dwFlags |= STARTF_FORCEOFFFEEDBACK;
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(30000);
    m_idle.setSingleShot(true);
    m_idle.setInterval(IdleShutdownMilliseconds);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_process.kill();
        failRequests();
    });
    connect(&m_idle, &QTimer::timeout, this, &TextImageCache::stopIdleHelper);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &TextImageCache::readResponse);
    connect(&m_process, &QProcess::errorOccurred, this, [this] { failRequests(); });
    connect(&m_process, &QProcess::finished, this, [this] { failRequests(); });
    // Drain stderr, but never confuse diagnostics with the binary response.
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        const QByteArray diagnostic = m_process.readAllStandardError().trimmed();
        if (!diagnostic.isEmpty()) {
            qWarning().noquote() << "Text image helper:" << diagnostic;
        }
    });
}

TextImageCache::~TextImageCache()
{
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

TextImageCache *TextImageCache::instance()
{
    static QPointer<TextImageCache> cache;
    if (!cache) {
        cache = new TextImageCache(QCoreApplication::instance());
    }
    return cache;
}

QByteArray TextImageCache::key(const QString &text,
                               const QFont &font,
                               qreal ratio,
                               const TextImageLayout &layout)
{
    // Resolve point sizes on the GUI screen before handing them to the helper.
    QFont resolved = font;
    if (resolved.pixelSize() < 0) {
        resolved.setPixelSize(qMax(1, QFontInfo(font).pixelSize()));
    }
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream << text << resolved << ratio << layout.width << layout.height << layout.flags
           << int(layout.elideMode);
    return result;
}

TextImageResult TextImageCache::lookup(const QByteArray &key) const
{
    const auto *result = m_results.object(key);
    return result ? *result : TextImageResult();
}

bool TextImageCache::pending(const QByteArray &key) const
{
    return m_pending.contains(key);
}

bool TextImageCache::helperRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void TextImageCache::setIdleShutdownInterval(int milliseconds)
{
    m_idle.setInterval(milliseconds);
}

void TextImageCache::request(const QByteArray &key)
{
    if (key.isEmpty() || lookup(key) || m_pending.contains(key)) {
        return;
    }
    if (m_failedAttempts.value(key) >= MaxAttempts) {
        // Out of attempts: the caller keeps the placeholder for this text.
        return;
    }
    if (m_pending.size() >= MaxOutstanding) {
        // The caller asks again as results arrive, so a big folder is rendered
        // in bounded batches instead of one burst.
        return;
    }
    m_pending.insert(key);
    submit(key);
}

void TextImageCache::submit(const QByteArray &key)
{
    m_idle.stop();
    if (m_process.state() == QProcess::NotRunning) {
        m_response.clear();
        m_process.start(QCoreApplication::applicationFilePath(),
                        {QStringLiteral("--text-image-helper")});
    }
    m_queue.enqueue(key);
    m_process.write(packet(key));
    if (!m_deadline.isActive()) {
        m_deadline.start();
    }
    StartupProfiler::mark("text-image.request");
}

void TextImageCache::complete(const QByteArray &key, const TextImageResult &result)
{
    if (!m_pending.remove(key)) {
        return;
    }
    const bool valid = result && result->images.size() == TextImages::ImageCount &&
                       std::all_of(result->images.cbegin(),
                                   result->images.cend(),
                                   [](const QImage &image) { return !image.isNull(); });
    if (valid) {
        m_failedAttempts.remove(key);
        qsizetype bytes = 0;
        for (const auto &image : result->images) {
            bytes += image.sizeInBytes();
        }
        m_results.insert(key, new TextImageResult(result), qMax(1, int(bytes / 1024)));
    } else {
        m_failedAttempts[key] += 1;
    }
    StartupProfiler::mark(valid ? "text-image.complete" : "text-image.failed");
    emit finished(key, valid ? result : TextImageResult());
}

void TextImageCache::readResponse()
{
    m_response += m_process.readAllStandardOutput();
    while (m_response.size() >= 4 && !m_queue.isEmpty()) {
        quint32 size;
        QDataStream header(m_response.first(4));
        header >> size;
        if (size > MaxPacket) {
            m_process.kill();
            failRequests();
            return;
        }
        if (m_response.size() < 4 + qsizetype(size)) {
            return;
        }
        QDataStream input(m_response.mid(4, size));
        auto result = QSharedPointer<TextImages>::create();
        input >> result->images;
        m_response.remove(0, 4 + size);
        const QByteArray key = m_queue.dequeue();
        // The device pixel ratio is the last field of the request key; the
        // helper's images carry no resolution of their own.
        QString name;
        QFont font;
        qreal ratio = 1;
        QDataStream request(key);
        request >> name >> font >> ratio;
        for (auto &image : result->images) {
            image.setDevicePixelRatio(ratio);
        }
        complete(key, input.status() == QDataStream::Ok ? result : TextImageResult());
        if (m_queue.isEmpty()) {
            m_deadline.stop();
            // Nothing is in flight: the helper may leave once it is not needed.
            m_idle.start();
        } else {
            m_deadline.start();
        }
    }
}

void TextImageCache::failRequests()
{
    m_deadline.stop();
    m_idle.stop();
    const auto requests = m_queue;
    m_queue.clear();
    m_response.clear();
    for (const auto &key : requests) {
        complete(key, {});
    }
}

void TextImageCache::stopIdleHelper()
{
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }
    // The helper leaves when its input ends; the next request starts it again.
    StartupProfiler::mark("text-image.idle-stop");
    m_process.closeWriteChannel();
}

bool isTextImageHelper(int argc, char **argv)
{
    return argc == 2 && QByteArray(argv[1]) == "--text-image-helper";
}

int runTextImageHelper(int argc, char **argv)
{
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    QGuiApplication application(argc, argv);
    for (;;) {
        char header[4];
        if (std::fread(header, 1, 4, stdin) != 4) {
            return 0; // parent's pipe was closed
        }
        quint32 size;
        QDataStream stream(QByteArray(header, 4));
        stream >> size;
        if (size > MaxPacket) {
            return 1;
        }
        QByteArray key(size, Qt::Uninitialized);
        if (std::fread(key.data(), 1, size, stdin) != size) {
            return 1;
        }
        const auto result = render(key);
        QByteArray payload;
        QDataStream output(&payload, QIODevice::WriteOnly);
        output << (result ? result->images : QList<QImage>());
        const QByteArray response = packet(payload);
        if (std::fwrite(response.constData(), 1, response.size(), stdout) !=
                size_t(response.size()) ||
            std::fflush(stdout) != 0) {
            return 1;
        }
    }
}
