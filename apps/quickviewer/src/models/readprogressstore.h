#ifndef READPROGRESSSTORE_H
#define READPROGRESSSTORE_H

#include <QtGui>
#include <QtCore>
#include <QtConcurrent>

#include "qvenums.h"

/**
 * Reading position and completion state for one volume.
 */
struct ReadProgress
{
    QString volumeTitle;
    QString volumePath;
    QString currentPageName;
    int totalPageCount;
    int resumePageIndex;
    bool completed;
};

Q_DECLARE_METATYPE(ReadProgress)

/**
 * Stores reading progress by volume path and persists it to progress.ini.
 */
class ReadProgressStore : public QObject
{
    Q_OBJECT
public:
    typedef QMap<QString, ReadProgress> ReadProgressMap;

    ReadProgressStore(QObject *parent);
    void save();

    static ReadProgressMap initializeAsync();

    bool contains(QString path)
    {
        return m_sessionOverrides.contains(path) || m_progressByVolumePath.contains(path);
    }
    ReadProgress at(QString path)
    {
        const auto sessionOverride = m_sessionOverrides.constFind(path);
        return sessionOverride == m_sessionOverrides.cend() ? m_progressByVolumePath[path]
                                                            : sessionOverride.value();
    }
    void insert(QString path, const ReadProgress &value)
    {
        m_progressByVolumePath.insert(path, value);
        emit progressChanged(path);
    }
    void insertSessionOverride(QString path, const ReadProgress &value)
    {
        m_sessionOverrides.insert(path, value);
        emit progressChanged(path);
    }
    void moveToThread(QThread *targetThread);

public slots:
    void handleInitializationFinished();

signals:
    /** The progress of the volume at \a path was inserted or replaced. */
    void progressChanged(QString path);
    /** The stored progress of every volume has been read from the settings. */
    void progressLoaded();

private:
    ReadProgressMap m_progressByVolumePath;
    ReadProgressMap m_sessionOverrides;
    QFutureWatcher<ReadProgressMap> m_initializeWatcher;
};

#endif // READPROGRESSSTORE_H
