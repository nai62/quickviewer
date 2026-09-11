
#include "readprogressstore.h"
#include "qvapplication.h"

ReadProgressStore::ReadProgressStore(QObject *parent)
    : QObject(parent)
{
    connect(&m_initializeWatcher, SIGNAL(finished()), SLOT(handleInitializationFinished()));
    QFuture<ReadProgressMap> future = QtConcurrent::run(&ReadProgressStore::initializeAsync);
    m_initializeWatcher.setFuture(future);
}

static QString getProgressIniPath()
{
    return qApp->getFilePathOfApplicationSetting(PROGRESS_INI);
}

void ReadProgressStore::save()
{
    QSettings settings(getProgressIniPath(), QSettings::IniFormat, this);
    //settings.setIniCodec(QTextCodec::codecForName("UTF-8"));

    QStringList groupNames;
    foreach (const ReadProgress &progress, m_progressByVolumePath.values()) {
        QString group = QString("Volume_%1").arg(groupNames.size() + 1, 4, 10, QChar('0'));
        settings.beginGroup(group);
        settings.setValue("Title", progress.volumeTitle);
        settings.setValue("Path", progress.volumePath);
        settings.setValue("CurrenPage", progress.currentPageName);
        settings.setValue("Pages", progress.totalPageCount);
        settings.setValue("Current", progress.resumePageIndex);
        settings.setValue("Completed", progress.completed);
        settings.endGroup();
        groupNames << group;
    }
    settings.sync();
}

ReadProgressStore::ReadProgressMap ReadProgressStore::initializeAsync()
{
    QSettings settings(getProgressIniPath(), QSettings::IniFormat);
    //settings.setIniCodec(QTextCodec::codecForName("UTF-8"));

    ReadProgressMap result;
    QStringList groups = settings.childGroups();
    foreach (const QString g, groups) {
        settings.beginGroup(g);
        QString path = settings.value("Path", "").toString();
        if (path.isEmpty()) {
            continue;
        }
        QString title = settings.value("Title", "").toString();
        QString currentPage = settings.value("CurrenPage", "").toString();
        int pages = settings.value("Pages", 0).toInt();
        int current = settings.value("Current", 0).toInt();
        bool completed = settings.value("Completed", false).toBool();
        ReadProgress progress = {
            title, path, currentPage, pages, current, completed};
        result[path] = progress;
        settings.endGroup();
    }
    return result;
}

void ReadProgressStore::handleInitializationFinished()
{
    m_progressByVolumePath = m_initializeWatcher.result();
}
