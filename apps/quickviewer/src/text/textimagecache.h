#ifndef TEXTIMAGECACHE_H
#define TEXTIMAGECACHE_H

#include <QtWidgets>

// Images, not QFont/QRawFont engines, cross the process boundary. The helper's
// font database cannot hold the GUI's font database mutex while finding glyphs.
struct TextImages
{
    // One coverage mask per weight, white on transparent. The widget tints a
    // mask with the colour it draws in, so the helper rasterizes a text twice
    // instead of once per palette colour.
    static constexpr int ImageCount = 2; // normal and bold
    QList<QImage> images;
};
using TextImageResult = QSharedPointer<const TextImages>;
Q_DECLARE_METATYPE(TextImageResult)

/**
 * \a mask tinted with \a color. The helper returns colourless coverage, so one
 * result serves every palette colour; the recent tints are kept because hover,
 * selection and a palette change repaint the widgets that draw them.
 */
QImage tintedTextMask(const QImage &mask, const QColor &color);

class TextImageCache : public QObject
{
    Q_OBJECT
public:
    explicit TextImageCache(QObject *parent = nullptr);
    ~TextImageCache() override;
    static TextImageCache *instance();
    /**
     * The key of one rasterization: the text, the resolved font and the scale,
     * plus the width the text is wrapped at. Zero means one line, elided to
     * what a list row can hold; a caption passes the width it has.
     */
    static QByteArray
    key(const QString &text, const QFont &font, qreal devicePixelRatio, int wrapWidth = 0);
    TextImageResult lookup(const QByteArray &key) const;
    /**
     * Asks the helper for the text behind \a key. A key that failed is retried
     * until \a MaxAttempts and then keeps the caller's placeholder.
     */
    void request(const QByteArray &key);
    bool pending(const QByteArray &key) const;
    /** True while the helper process runs. */
    bool helperRunning() const;
    /** Requests accepted before the caller has to ask again as results arrive. */
    static constexpr int MaxOutstanding = 8;
    /** Attempts one key gets before it keeps the placeholder for the session. */
    static constexpr int MaxAttempts = 2;
    /** How long the helper is kept after its last request, in milliseconds. */
    void setIdleShutdownInterval(int milliseconds);

signals:
    void finished(const QByteArray &key, const TextImageResult &result);

protected:
    // Overridable transport lets tests delay completion without real fonts or sleeps.
    virtual void submit(const QByteArray &key);
    void complete(const QByteArray &key, const TextImageResult &result);

private:
    void readResponse();
    void failRequests();
    void stopIdleHelper();
    QProcess m_process;
    QTimer m_deadline;
    QTimer m_idle;
    QByteArray m_response;
    QQueue<QByteArray> m_queue;
    QSet<QByteArray> m_pending;
    QHash<QByteArray, int> m_failedAttempts;
    QCache<QByteArray, TextImageResult> m_results;
};

// Called before constructing the normal application (also by the GUI tests).
bool isTextImageHelper(int argc, char **argv);
int runTextImageHelper(int argc, char **argv);

#endif
