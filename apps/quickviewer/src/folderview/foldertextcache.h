#ifndef FOLDERTEXTCACHE_H
#define FOLDERTEXTCACHE_H

#include <QtWidgets>

// Images, not QFont/QRawFont engines, cross the process boundary. The helper's
// font database cannot hold the GUI's font database mutex while finding glyphs.
struct FolderTextImages
{
    // One coverage mask per weight, white on transparent. The delegate tints a
    // mask with the colour its row needs, so the helper rasterizes a name twice
    // instead of once per palette colour.
    static constexpr int ImageCount = 2; // normal and bold
    QList<QImage> images;
};
using FolderTextResult = QSharedPointer<const FolderTextImages>;
Q_DECLARE_METATYPE(FolderTextResult)

class FolderTextCache : public QObject
{
    Q_OBJECT
public:
    explicit FolderTextCache(QObject *parent = nullptr);
    ~FolderTextCache() override;
    static FolderTextCache *instance();
    static QByteArray key(const QString &text, const QFont &font, qreal devicePixelRatio);
    FolderTextResult lookup(const QByteArray &key) const;
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
    void finished(const QByteArray &key, const FolderTextResult &result);

protected:
    // Overridable transport lets tests delay completion without real fonts or sleeps.
    virtual void submit(const QByteArray &key);
    void complete(const QByteArray &key, const FolderTextResult &result);

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
    QCache<QByteArray, FolderTextResult> m_results;
};

// Called before constructing the normal application (also by the GUI tests).
bool isFolderTextHelper(int argc, char **argv);
int runFolderTextHelper(int argc, char **argv);

#endif
