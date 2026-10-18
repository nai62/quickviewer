#include "foldertextcache.h"
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

FolderTextResult render(const QByteArray &key)
{
    QString text;
    QFont font;
    qreal ratio = 1;
    int wrapWidth = 0;
    QDataStream input(key);
    input >> text >> font >> ratio >> wrapWidth;
    if (input.status() != QDataStream::Ok || !qIsFinite(ratio) || ratio < 0.5 || ratio > 8 ||
        text.size() > 32768 || wrapWidth < 0 || wrapWidth > MaxWrapWidth) {
        return {};
    }
    auto result = QSharedPointer<FolderTextImages>::create();
    for (int bold = 0; bold < FolderTextImages::ImageCount; ++bold) {
        if (bold) {
            font.setBold(true);
        }
        // A small logical paint device transfers the actual view's DPI. Font
        // point sizes must not depend on the helper's default screen.
        QImage device(1, 1, QImage::Format_ARGB32_Premultiplied);
        device.setDevicePixelRatio(ratio);
        QFontMetrics metrics(font, &device);
        // A caption wraps the path the way the label does, so the image covers
        // the lines the label draws; a name gets one line, elided to the width
        // a list row can hold.
        const bool wrapped = wrapWidth > 0;
        const QString visible =
            wrapped ? QString() : metrics.elidedText(text, Qt::ElideRight, 4096);
        const QRect lines = wrapped ? metrics.boundingRect(QRect(0, 0, wrapWidth, MaxWrapHeight),
                                                           Qt::TextWordWrap,
                                                           text)
                                    : QRect();
        const int width =
            qBound(1, (wrapped ? lines.width() : metrics.horizontalAdvance(visible)) + 4, 8192);
        const int height = qBound(1, (wrapped ? lines.height() : metrics.height()) + 4, 16384);
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
        if (wrapped) {
            painter.drawText(QRect(2, 2, wrapWidth, lines.height()), Qt::TextWordWrap, text);
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

FolderTextCache::FolderTextCache(QObject *parent)
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
    connect(&m_idle, &QTimer::timeout, this, &FolderTextCache::stopIdleHelper);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &FolderTextCache::readResponse);
    connect(&m_process, &QProcess::errorOccurred, this, [this] { failRequests(); });
    connect(&m_process, &QProcess::finished, this, [this] { failRequests(); });
    // Drain stderr, but never confuse diagnostics with the binary response.
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        const QByteArray diagnostic = m_process.readAllStandardError().trimmed();
        if (!diagnostic.isEmpty()) {
            qWarning().noquote() << "Folder text helper:" << diagnostic;
        }
    });
}

FolderTextCache::~FolderTextCache()
{
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

FolderTextCache *FolderTextCache::instance()
{
    static QPointer<FolderTextCache> cache;
    if (!cache) {
        cache = new FolderTextCache(QCoreApplication::instance());
    }
    return cache;
}

QByteArray FolderTextCache::key(const QString &text, const QFont &font, qreal ratio, int wrapWidth)
{
    // Resolve point sizes on the GUI screen before handing them to the helper.
    QFont resolved = font;
    if (resolved.pixelSize() < 0) {
        resolved.setPixelSize(qMax(1, QFontInfo(font).pixelSize()));
    }
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream << text << resolved << ratio << wrapWidth;
    return result;
}

FolderTextResult FolderTextCache::lookup(const QByteArray &key) const
{
    const auto *result = m_results.object(key);
    return result ? *result : FolderTextResult();
}

bool FolderTextCache::pending(const QByteArray &key) const
{
    return m_pending.contains(key);
}

bool FolderTextCache::helperRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void FolderTextCache::setIdleShutdownInterval(int milliseconds)
{
    m_idle.setInterval(milliseconds);
}

void FolderTextCache::request(const QByteArray &key)
{
    if (key.isEmpty() || lookup(key) || m_pending.contains(key)) {
        return;
    }
    if (m_failedAttempts.value(key) >= MaxAttempts) {
        // Out of attempts: the caller keeps the placeholder for this name.
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

void FolderTextCache::submit(const QByteArray &key)
{
    m_idle.stop();
    if (m_process.state() == QProcess::NotRunning) {
        m_response.clear();
        m_process.start(QCoreApplication::applicationFilePath(),
                        {QStringLiteral("--folder-text-helper")});
    }
    m_queue.enqueue(key);
    m_process.write(packet(key));
    if (!m_deadline.isActive()) {
        m_deadline.start();
    }
    StartupProfiler::mark("folder-text.request");
}

void FolderTextCache::complete(const QByteArray &key, const FolderTextResult &result)
{
    if (!m_pending.remove(key)) {
        return;
    }
    const bool valid = result && result->images.size() == FolderTextImages::ImageCount &&
                       std::all_of(result->images.cbegin(),
                                   result->images.cend(),
                                   [](const QImage &image) { return !image.isNull(); });
    if (valid) {
        m_failedAttempts.remove(key);
        qsizetype bytes = 0;
        for (const auto &image : result->images) {
            bytes += image.sizeInBytes();
        }
        m_results.insert(key, new FolderTextResult(result), qMax(1, int(bytes / 1024)));
    } else {
        m_failedAttempts[key] += 1;
    }
    StartupProfiler::mark(valid ? "folder-text.complete" : "folder-text.failed");
    emit finished(key, valid ? result : FolderTextResult());
}

void FolderTextCache::readResponse()
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
        auto result = QSharedPointer<FolderTextImages>::create();
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
        complete(key, input.status() == QDataStream::Ok ? result : FolderTextResult());
        if (m_queue.isEmpty()) {
            m_deadline.stop();
            // Nothing is in flight: the helper may leave once it is not needed.
            m_idle.start();
        } else {
            m_deadline.start();
        }
    }
}

void FolderTextCache::failRequests()
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

void FolderTextCache::stopIdleHelper()
{
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }
    // The helper leaves when its input ends; the next request starts it again.
    StartupProfiler::mark("folder-text.idle-stop");
    m_process.closeWriteChannel();
}

bool isFolderTextHelper(int argc, char **argv)
{
    return argc == 2 && QByteArray(argv[1]) == "--folder-text-helper";
}

int runFolderTextHelper(int argc, char **argv)
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
