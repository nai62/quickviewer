#include "rarextractor.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTextStream>

#include <cstdio>

struct BenchmarkCaseResult
{
    bool success = false;
    qint64 elapsedNs = 0;
    qint64 bytes = 0;
    RarArchiveError error = RarArchiveError::None;
    RarAccessStatistics statistics;
};

static QString errorName(RarArchiveError error)
{
    switch (error) {
    case RarArchiveError::None:
        return QStringLiteral("none");
    case RarArchiveError::PasswordProtected:
        return QStringLiteral("password");
    case RarArchiveError::Unsupported:
        return QStringLiteral("unsupported");
    case RarArchiveError::Corrupt:
        return QStringLiteral("corrupt");
    case RarArchiveError::IoError:
        return QStringLiteral("io");
    }
    return QStringLiteral("unknown");
}

static void printHelp(QTextStream &out)
{
    out << "Usage: rar-benchmark <archive.rar> [archive2.rar ...]" << '\n'
        << '\n'
        << "Benchmarks RAR archive access patterns used by QuickViewer." << '\n'
        << '\n'
        << "Options:" << '\n'
        << "  -h, --help, /?    Show this help." << '\n';
}

static BenchmarkCaseResult runReads(
    const QString &archiveName, const QStringList &warmupFiles, const QStringList &measuredFiles)
{
    BenchmarkCaseResult result;
    RarExtractor extractor(archiveName);
    if (!extractor.open(RarExtractor::OpenModeList)) {
        result.error = extractor.archiveError();
        result.statistics = extractor.statistics();
        return result;
    }

    for (const QString &fileName : warmupFiles) {
        const RarFileDataResult warmup = extractor.fileDataResult(fileName);
        if (!warmup.success) {
            result.error = warmup.error;
            result.statistics = extractor.statistics();
            return result;
        }
    }

    extractor.resetStatistics();
    QElapsedTimer timer;
    timer.start();
    for (const QString &fileName : measuredFiles) {
        const RarFileDataResult read = extractor.fileDataResult(fileName);
        if (!read.success) {
            result.elapsedNs = timer.nsecsElapsed();
            result.error = read.error;
            result.statistics = extractor.statistics();
            return result;
        }
        result.bytes += read.data.size();
    }
    result.elapsedNs = timer.nsecsElapsed();
    result.statistics = extractor.statistics();
    result.success = true;
    return result;
}

static void printCase(QTextStream &out, const QString &name, const BenchmarkCaseResult &result)
{
    out << name << ": "
        << QString::number(static_cast<double>(result.elapsedNs) / 1000000.0, 'f', 3)
        << " ms"
        << ", bytes=" << result.bytes
        << ", reopenCount=" << result.statistics.reopenCount
        << ", readHeaderCount=" << result.statistics.readHeaderCount
        << ", skipCount=" << result.statistics.skipCount
        << ", extractCount=" << result.statistics.extractCount
        << ", cacheHitCount=" << result.statistics.cacheHitCount
        << ", success=" << (result.success ? "true" : "false")
        << ", error=" << errorName(result.error) << '\n';
}

static bool benchmarkArchive(const QString &archiveName, QTextStream &out)
{
    RarExtractor extractor(archiveName);
    QElapsedTimer listTimer;
    listTimer.start();
    const bool opened = extractor.open(RarExtractor::OpenModeList);

    BenchmarkCaseResult listResult;
    listResult.elapsedNs = listTimer.nsecsElapsed();
    listResult.success = opened;
    listResult.error = extractor.archiveError();
    listResult.statistics = extractor.statistics();

    out << "archive: " << QFileInfo(archiveName).absoluteFilePath() << '\n';
    if (!opened) {
        printCase(out, QStringLiteral("list"), listResult);
        out << '\n';
        return false;
    }

    QStringList files;
    for (const RARFileInfo &info : extractor.fileInfoList()) {
        if (!info.isDirectory()) {
            files.append(info.fileName);
        }
    }

    out << "solid: " << (extractor.isSolid() ? "true" : "false")
        << ", entries=" << extractor.fileInfoList().size()
        << ", files=" << files.size() << '\n';
    printCase(out, QStringLiteral("list"), listResult);

    if (files.isEmpty()) {
        out << "No file entries to benchmark." << '\n'
            << '\n';
        return true;
    }

    const int middle = files.size() / 2;
    const int lower = files.size() / 3;
    int upper = (files.size() * 2) / 3;
    if (upper == lower && files.size() > 1) {
        upper = files.size() - 1;
    }

    printCase(
        out,
        QStringLiteral("cold read"),
        runReads(archiveName, {}, {files.at(middle)}));

    if (lower != upper) {
        printCase(
            out,
            QStringLiteral("forward"),
            runReads(archiveName, {files.at(lower)}, {files.at(upper)}));
        printCase(
            out,
            QStringLiteral("backward"),
            runReads(archiveName, {files.at(upper)}, {files.at(lower)}));
    } else {
        out << "forward: n/a (requires at least two distinct entries)" << '\n';
        out << "backward: n/a (requires at least two distinct entries)" << '\n';
    }

    printCase(
        out,
        QStringLiteral("sequence"),
        runReads(archiveName, {}, files));
    printCase(
        out,
        QStringLiteral("cache hit"),
        runReads(archiveName, {files.at(middle)}, {files.at(middle)}));
    out << '\n';
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList arguments = app.arguments();
    if (arguments.contains(QStringLiteral("--help")) || arguments.contains(QStringLiteral("-h")) || arguments.contains(QStringLiteral("/?"))) {
        printHelp(out);
        return 0;
    }
    if (arguments.size() < 2) {
        err << "Usage: rar-benchmark <archive.rar> [archive2.rar ...]" << '\n';
        return 2;
    }

    bool success = true;
    for (int index = 1; index < arguments.size(); ++index) {
        success = benchmarkArchive(arguments.at(index), out) && success;
    }
    return success ? 0 : 1;
}
