#include "startupprofiler.h"

#include <QtCore>
#include <chrono>

namespace {
using Clock = std::chrono::steady_clock;

struct Record
{
    qint64 elapsedMicroseconds;
    quintptr threadId;
    QByteArray label;
};

Clock::time_point origin = Clock::now();
QMutex recordsMutex;
QVector<Record> records;
}

void StartupProfiler::start()
{
    if (!enabled()) {
        return;
    }
    origin = Clock::now();
    QMutexLocker locker(&recordsMutex);
    records.clear();
    locker.unlock();
    mark("main.entry");
}

bool StartupProfiler::enabled()
{
    return !qgetenv("QV_PROFILE_FIRST_IMAGE").isEmpty();
}

void StartupProfiler::mark(const char *label)
{
    if (!enabled()) {
        return;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             Clock::now() - origin)
                             .count();
    QMutexLocker locker(&recordsMutex);
    records.push_back({elapsed,
                       reinterpret_cast<quintptr>(QThread::currentThreadId()),
                       QByteArray(label)});
}

void StartupProfiler::flush()
{
    const QByteArray outputPath = qgetenv("QV_PROFILE_FIRST_IMAGE");
    if (outputPath.isEmpty()) {
        return;
    }
    QVector<Record> snapshot;
    {
        QMutexLocker locker(&recordsMutex);
        snapshot = records;
    }
    QFile output(QString::fromLocal8Bit(outputPath));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&output);
    for (const Record &record : snapshot) {
        stream << record.elapsedMicroseconds << '\t' << record.threadId
               << '\t' << record.label << '\n';
    }
}
