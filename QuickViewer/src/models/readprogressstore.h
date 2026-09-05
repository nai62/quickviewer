#ifndef READPROGRESSSTORE_H
#define READPROGRESSSTORE_H

#include <QtGui>
#include <QtCore>
#include <QtConcurrent>

#include "qv_init.h"

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

    bool contains(QString path) { return m_progressByVolumePath.contains(path); }
    ReadProgress at(QString path) { return m_progressByVolumePath[path]; }
    void insert(QString path, const ReadProgress &value) { m_progressByVolumePath.insert(path, value); }
    void moveToThread(QThread *targetThread);

public slots:
    void handleInitializationFinished();

private:
    ReadProgressMap m_progressByVolumePath;
    QFutureWatcher<ReadProgressMap> m_initializeWatcher;
};

#endif // READPROGRESSSTORE_H
