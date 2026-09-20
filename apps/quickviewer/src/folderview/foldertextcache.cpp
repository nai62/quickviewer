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
    QList<QColor> colors;
    qreal ratio;
    QDataStream input(key);
    input >> text >> font >> colors >> ratio;
    if (input.status() != QDataStream::Ok || colors.size() != FolderTextImages::ColorCount ||
        !qIsFinite(ratio) || ratio < 0.5 || ratio > 8 || text.size() > 32768) {
        return {};
    }
    auto result = QSharedPointer<FolderTextImages>::create();
    for (int bold = 0; bold < 2; ++bold) {
        if (bold) {
            font.setBold(true);
        }
        // A small logical paint device transfers the actual view's DPI. Font
        // point sizes must not depend on the helper's default screen.
        QImage device(1, 1, QImage::Format_ARGB32_Premultiplied);
        device.setDevicePixelRatio(ratio);
        QFontMetrics metrics(font, &device);
        const QString visible = metrics.elidedText(text, Qt::ElideRight, 4096);
        const int width = qBound(1, metrics.horizontalAdvance(visible) + 4, 8192);
        const int height = qBound(1, metrics.height() + 4, 512);
        for (const QColor &color : colors) {
            QImage image(QSize(qCeil(width * ratio), qCeil(height * ratio)),
                         QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(ratio);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setFont(font);
            painter.setPen(color);
            painter.drawText(QPoint(2, 2 + metrics.ascent()), visible);
            painter.end();
            result->images.append(image);
        }
    }
    return result;
}
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
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_process.kill();
        failRequests();
    });
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

QByteArray
FolderTextCache::key(const QString &text, const QFont &font, const QPalette &palette, qreal ratio)
{
    // Resolve point sizes on the GUI screen before handing them to the helper.
    QFont resolved = font;
    if (resolved.pixelSize() < 0) {
        resolved.setPixelSize(qMax(1, QFontInfo(font).pixelSize()));
    }
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream << text << resolved
           << QList<QColor>{palette.color(QPalette::Active, QPalette::Text),
                            palette.color(QPalette::Active, QPalette::HighlightedText),
                            palette.color(QPalette::Inactive, QPalette::Text),
                            palette.color(QPalette::Inactive, QPalette::HighlightedText),
                            palette.color(QPalette::Disabled, QPalette::Text),
                            palette.color(QPalette::Disabled, QPalette::HighlightedText)}
           << ratio;
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

void FolderTextCache::request(const QByteArray &key)
{
    if (lookup(key) || m_pending.contains(key) || m_failed.contains(key)) {
        return;
    }
    m_pending.insert(key);
    submit(key);
}

void FolderTextCache::submit(const QByteArray &key)
{
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
        qsizetype bytes = 0;
        for (const auto &image : result->images) {
            bytes += image.sizeInBytes();
        }
        m_results.insert(key, new FolderTextResult(result), qMax(1, int(bytes / 1024)));
    } else {
        m_failed.insert(key);
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
        QString name;
        QFont font;
        QList<QColor> colors;
        qreal ratio = 1;
        QDataStream request(key);
        request >> name >> font >> colors >> ratio;
        for (auto &image : result->images) {
            image.setDevicePixelRatio(ratio);
        }
        complete(key, input.status() == QDataStream::Ok ? result : FolderTextResult());
        if (m_queue.isEmpty()) {
            m_deadline.stop();
        } else {
            m_deadline.start();
        }
    }
}

void FolderTextCache::failRequests()
{
    m_deadline.stop();
    const auto requests = m_queue;
    m_queue.clear();
    m_response.clear();
    for (const auto &key : requests) {
        complete(key, {});
    }
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
