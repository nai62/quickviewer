#ifndef FOLDERTEXTCACHE_H
#define FOLDERTEXTCACHE_H

#include <QtWidgets>

// Images, not QFont/QRawFont engines, cross the process boundary. The helper's
// font database cannot hold the GUI's font database mutex while finding glyphs.
struct FolderTextImages
{
    static constexpr int ColorCount = 6; // Text/HighlightedText for all three palette groups
    static constexpr int ImageCount = 2 * ColorCount; // normal and bold
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
    static QByteArray
    key(const QString &text, const QFont &font, const QPalette &palette, qreal devicePixelRatio);
    FolderTextResult lookup(const QByteArray &key) const;
    void request(const QByteArray &key);
    bool pending(const QByteArray &key) const;

signals:
    void finished(const QByteArray &key, const FolderTextResult &result);

protected:
    // Overridable transport lets tests delay completion without real fonts or sleeps.
    virtual void submit(const QByteArray &key);
    void complete(const QByteArray &key, const FolderTextResult &result);

private:
    void readResponse();
    void failRequests();
    QProcess m_process;
    QTimer m_deadline;
    QByteArray m_response;
    QQueue<QByteArray> m_queue;
    QSet<QByteArray> m_pending;
    QSet<QByteArray> m_failed;
    QCache<QByteArray, FolderTextResult> m_results;
};

// Called before constructing the normal application (also by the GUI tests).
bool isFolderTextHelper(int argc, char **argv);
int runFolderTextHelper(int argc, char **argv);

#endif
