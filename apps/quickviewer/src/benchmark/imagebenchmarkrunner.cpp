#include "imagebenchmarkrunner.h"

#include <algorithm>
#include <functional>
#include <memory>

#include <QtCore>

#include "fileloader.h"
#include "fileloader7zarchive.h"
#include "imageloadmetrics.h"
#include "imageformat.h"
#include "qvapplication.h"
#include "readprogressstore.h"
#include "volume.h"
#include "volumeloader.h"

namespace {

constexpr const char *FirstPaintChildEnv = "QV_BENCHMARK_FIRST_PAINT_CHILD";
constexpr const char *EmptyWindowChildEnv = "QV_BENCHMARK_EMPTY_WINDOW_CHILD";
constexpr const char *SortEnv = "QV_BENCHMARK_SORT";
constexpr const char *RecursiveEnv = "QV_BENCHMARK_RECURSIVE";
constexpr const char *PageIndexEnv = "QV_BENCHMARK_PAGE_INDEX";
constexpr const char *JpegDecoderEnv = "QV_BENCHMARK_JPEG_DECODER";
constexpr const char *PngDecoderEnv = "QV_BENCHMARK_PNG_DECODER";
constexpr const char *WebPDecoderEnv = "QV_BENCHMARK_WEBP_DECODER";

enum class BenchmarkSuite {
    Decode,
    EntryLoad,
    ArchiveOpen,
    FirstImage,
    FirstPaint,
    EmptyWindow,
};

enum class PageSelectionKind {
    First,
    Resume,
    Index,
};

struct PageSelection
{
    PageSelectionKind kind = PageSelectionKind::First;
    int index = 0;
    QString requested = "first";
};

struct BenchmarkOptions
{
    BenchmarkSuite suite = BenchmarkSuite::Decode;
    QStringList inputs;
    QString outputPath;
    bool recursive = false;
    int runs = 5;
    int warmup = 2;
    PageSelection page;
    qvEnums::ImageSortBy sort = qvEnums::ImageSortBy::SortByFileName;
    QString sortName = "name";
    QMap<QString, QStringList> decoderBackends;
    bool showHelp = false;
};

struct PageResolution
{
    bool success = false;
    int index = -1;
    QString source;
    QString error;
};

struct BenchmarkRecord
{
    QString suite;
    QString input;
    int run = 0;
    bool success = false;
    QString error;

    QString container;
    qint64 archiveSize = -1;
    qint64 archiveEntryCount = -1;
    qint64 imageCount = -1;
    QString entry;
    qint64 entryUncompressedSize = -1;
    QString format;
    QSize sourceSize;

    QString requestedPage;
    int resolvedPage = -1;
    QString pageSource;

    QString requestedDecoder;
    QString actualDecoder;
    QString decoderFallbackReason;
    QString sort;

    QMap<QString, qint64> profileMilestoneNanoseconds;

    qint64 libraryInitNanoseconds = -1;
    qint64 archiveOpenNanoseconds = -1;
    qint64 enumerationNanoseconds = -1;
    qint64 filterNanoseconds = -1;
    qint64 archiveSortNanoseconds = -1;
    qint64 pageListNanoseconds = -1;
    qint64 pageSortNanoseconds = -1;
    qint64 pageSelectNanoseconds = -1;
    qint64 sourceLoadNanoseconds = -1;
    qint64 extractNanoseconds = -1;
    qint64 decodeNanoseconds = -1;
    qint64 postprocessNanoseconds = -1;
    qint64 decodePipelineNanoseconds = -1;
    qint64 totalNanoseconds = -1;
};

struct ProfileMilestone
{
    const char *label;
    const char *csvColumn;
};

const ProfileMilestone FirstPaintMilestones[] = {
    {"application.construct.begin", "application_construct_begin_at_us"},
    {"application.base-ready", "application_base_ready_at_us"},
    {"application.settings-opened", "application_settings_opened_at_us"},
    {"application.languages-ready", "application_languages_ready_at_us"},
    {"application.keymap-ready", "application_keymap_ready_at_us"},
    {"application.ini-read.begin", "application_ini_read_begin_at_us"},
    {"application.ini-read.end", "application_ini_read_end_at_us"},
    {"application.locale-ready", "application_locale_ready_at_us"},
    {"application.screen-ready", "application_screen_ready_at_us"},
    {"application.pictures-folder-ready", "application_pictures_folder_ready_at_us"},
    {"application.settings-read", "application_settings_read_at_us"},
    {"application.theme-ready", "application_theme_ready_at_us"},
    {"application.settings-loaded", "application_settings_loaded_at_us"},
    {"application.construct.end", "application_construct_end_at_us"},
    {"application.constructed", "application_constructed_at_us"},
    {"mainwindow.construct.begin", "mainwindow_construct_begin_at_us"},
    {"mainwindow.ui-setup", "mainwindow_ui_setup_at_us"},
    {"mainwindow.actions-registered", "mainwindow_actions_registered_at_us"},
    {"mainwindow.initial-message.begin", "mainwindow_initial_message_begin_at_us"},
    {"mainwindow.initial-message.end", "mainwindow_initial_message_end_at_us"},
    {"mainwindow.page-bar-sync.begin", "mainwindow_page_bar_sync_begin_at_us"},
    {"mainwindow.page-bar-sync.end", "mainwindow_page_bar_sync_end_at_us"},
    {"mainwindow.construct.end", "mainwindow_construct_end_at_us"},
    {"mainwindow.constructed", "mainwindow_constructed_at_us"},
    {"startup.window-state-restored", "window_state_restored_at_us"},
    {"startup.cloak.begin", "cloak_begin_at_us"},
    {"startup.cloak.before-winid", "cloak_before_winid_at_us"},
    {"startup.cloak.after-winid", "cloak_after_winid_at_us"},
    {"startup.cloak.end", "cloak_end_at_us"},
    {"startup.show.begin", "show_begin_at_us"},
    {"startup.show.end", "show_end_at_us"},
    {"startup.window-shown", "window_shown_at_us"},
    {"startup.panel-reserve.begin", "panel_reserve_begin_at_us"},
    {"folder-window.construct.begin", "folder_window_construct_begin_at_us"},
    {"folder-item-icons.begin", "folder_item_icons_begin_at_us"},
    {"folder-item-icons.end", "folder_item_icons_end_at_us"},
    {"folder-window.history-button.begin", "folder_window_history_button_begin_at_us"},
    {"folder-window.history-button.end", "folder_window_history_button_end_at_us"},
    {"folder-window.construct.end", "folder_window_construct_end_at_us"},
    {"startup.panel-reserve.end", "panel_reserve_end_at_us"},
    {"startup.panel-ready", "panel_ready_at_us"},
    {"startup.process-events.begin", "process_events_begin_at_us"},
    {"startup.process-events.end", "process_events_end_at_us"},
    {"startup.initial-events-processed", "initial_events_processed_at_us"},
    {"startup.reveal.begin", "reveal_begin_at_us"},
    {"startup.opacity-restore.begin", "opacity_restore_begin_at_us"},
    {"startup.opacity-restore.end", "opacity_restore_end_at_us"},
    {"startup.repaint.begin", "repaint_begin_at_us"},
    {"startup.repaint.end", "repaint_end_at_us"},
    {"startup.uncloak.begin", "uncloak_begin_at_us"},
    {"startup.uncloak.end", "uncloak_end_at_us"},
    {"startup.reveal.end", "reveal_end_at_us"},
    {"startup.reveal.end", "first_image_visible_at_us"},
    {"startup.initialized", "startup_initialized_at_us"},
    {"catalog-database.constructed", "catalog_database_constructed_at_us"},
    {"catalog-database.attached", "catalog_database_attached_at_us"},
    {"event-loop.begin", "event_loop_begin_at_us"},
    {"startup-volume.begin", "startup_volume_begin_at_us"},
    {"session.load-volume.begin", "session_load_volume_begin_at_us"},
    {"volume-loader.begin", "volume_loader_begin_at_us"},
    {"volume-loader.created", "volume_loader_created_at_us"},
    {"volume.page-list.begin", "page_list_begin_at_us"},
    {"volume.page-list.end", "page_list_end_at_us"},
    {"volume-loader.page-list-loaded", "volume_loader_page_list_loaded_at_us"},
    {"session.volume-built", "session_volume_built_at_us"},
    {"session.select-page.begin", "session_select_page_begin_at_us"},
    {"session.prefetch-scheduled", "session_prefetch_scheduled_at_us"},
    {"startup-volume.prefetch.begin", "volume_prefetch_begin_at_us"},
    {"startup-volume.prefetch.page-ready", "volume_prefetch_page_ready_at_us"},
    {"image-worker.extract.begin", "extract_begin_at_us"},
    {"image-worker.extract.end", "extract_end_at_us"},
    {"image-worker.decode-resize.end", "decode_resize_end_at_us"},
    {"session.first-image-ready", "session_first_image_ready_at_us"},
    {"first-image-painted", "first_image_painted_at_us"},
};

struct PreparedFirstPaintInput
{
    bool success = false;
    QString childInput;
    QString selectedEntry;
    QString format;
    int resolvedPage = -1;
    QString pageSource;
    QString container;
    qint64 archiveSize = -1;
    qint64 imageCount = -1;
    qint64 entryUncompressedSize = -1;
    QString error;
};

struct FileSnapshot
{
    QString path;
    QByteArray data;
    bool existed = false;
    bool valid = false;
};

using ByteLoader = std::function<QByteArray()>;

QString suiteName(BenchmarkSuite suite)
{
    switch (suite) {
    case BenchmarkSuite::Decode:
        return "decode";
    case BenchmarkSuite::EntryLoad:
        return "entry-load";
    case BenchmarkSuite::ArchiveOpen:
        return "archive-open";
    case BenchmarkSuite::FirstImage:
        return "first-image";
    case BenchmarkSuite::FirstPaint:
        return "first-paint";
    case BenchmarkSuite::EmptyWindow:
        return "empty-window";
    }
    return "unknown";
}

bool parseSuite(const QString &text, BenchmarkSuite &suite)
{
    if (text == "decode") {
        suite = BenchmarkSuite::Decode;
        return true;
    }
    if (text == "entry-load") {
        suite = BenchmarkSuite::EntryLoad;
        return true;
    }
    if (text == "archive-open") {
        suite = BenchmarkSuite::ArchiveOpen;
        return true;
    }
    if (text == "first-image") {
        suite = BenchmarkSuite::FirstImage;
        return true;
    }
    if (text == "first-paint") {
        suite = BenchmarkSuite::FirstPaint;
        return true;
    }
    if (text == "empty-window") {
        suite = BenchmarkSuite::EmptyWindow;
        return true;
    }
    return false;
}

bool parseSortMode(const QString &text, qvEnums::ImageSortBy &sort)
{
    if (text == "name") {
        sort = qvEnums::ImageSortBy::SortByFileName;
        return true;
    }
    if (text == "name-desc") {
        sort = qvEnums::ImageSortBy::SortByFileNameDescending;
        return true;
    }
    if (text == "size") {
        sort = qvEnums::ImageSortBy::SortByFileSize;
        return true;
    }
    if (text == "size-desc") {
        sort = qvEnums::ImageSortBy::SortByFileSizeDescending;
        return true;
    }
    if (text == "mtime") {
        sort = qvEnums::ImageSortBy::SortByModifiedTime;
        return true;
    }
    if (text == "mtime-desc") {
        sort = qvEnums::ImageSortBy::SortByModifiedTimeDescending;
        return true;
    }
    return false;
}

QString archiveContainerName(const QString &path)
{
    const QFileInfo info(path);
    const QString completeSuffix = info.completeSuffix().toLower();
    if (completeSuffix.endsWith("cbz")) {
        return "zip";
    }
    if (completeSuffix.endsWith("cbr")) {
        return "rar";
    }
    if (completeSuffix.endsWith("cb7")) {
        return "7z";
    }
    if (completeSuffix.endsWith("tar.gz")) {
        return "tgz";
    }
    if (completeSuffix.endsWith("tar.bz2")) {
        return "tbz2";
    }
    if (completeSuffix.endsWith("tar.xz")) {
        return "txz";
    }
    return info.suffix().toLower();
}

QString containerNameForInput(const QString &input, const Volume *volume = nullptr)
{
    if (volume && volume->isArchive()) {
        return archiveContainerName(volume->volumePath());
    }
    const QFileInfo info(input);
    if (info.isDir()) {
        return "directory";
    }
    if (info.isFile() && IFileLoader::isImageFile(input)) {
        return "file";
    }
    return archiveContainerName(input);
}

bool isDirectImageInput(const QString &input)
{
    const QFileInfo info(input);
    return info.isFile() && IFileLoader::isImageFile(input);
}

bool isDecodedImageValid(const ImageContent &content)
{
    return !content.loadedImage.isNull() || !content.resizedImage.isNull() ||
           !content.movie.isNull();
}

QString fallbackReason(const QString &requested, const QString &actual)
{
    if (requested.isEmpty() || requested.endsWith("=auto") || actual.isEmpty()) {
        return QString();
    }
    const QString backend = requested.section('=', 1, 1);
    if (backend == "qt" && (actual.startsWith("qimagereader:", Qt::CaseInsensitive) ||
                            actual.startsWith("qmovie:", Qt::CaseInsensitive))) {
        return QString();
    }
    if (actual.contains(backend, Qt::CaseInsensitive)) {
        return QString();
    }
    return "backend-unavailable-or-incompatible-input";
}

bool parsePositiveInteger(const QString &value, int &result)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok || parsed <= 0) {
        return false;
    }
    result = parsed;
    return true;
}

bool parseNonNegativeInteger(const QString &value, int &result)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok || parsed < 0) {
        return false;
    }
    result = parsed;
    return true;
}

bool validateDecoderBackend(const QString &format, const QString &backend)
{
    if (backend == "auto" || backend == "qt") {
        return true;
    }
    if (format == "jpeg") {
        return backend == "turbojpeg";
    }
    if (format == "png") {
        return backend == "libspng";
    }
    if (format == "webp") {
        return backend == "libwebp";
    }
    return false;
}

bool parseDecoderSpec(const QString &spec, QString &format, QStringList &backends, QString &error)
{
    const int equals = spec.indexOf('=');
    if (equals <= 0 || equals == spec.size() - 1) {
        error = QString("Invalid --decoder value '%1'. Expected FORMAT=BACKEND[,BACKEND...].")
                    .arg(spec);
        return false;
    }
    format = spec.left(equals).trimmed().toLower();
    if (format == "jpg") {
        format = "jpeg";
    }
    if (format != "jpeg" && format != "png" && format != "webp") {
        error =
            QString("Unsupported decoder format '%1'. Supported formats are jpeg, png, and webp.")
                .arg(format);
        return false;
    }
    const QStringList rawBackends = spec.mid(equals + 1).split(',', Qt::SkipEmptyParts);
    if (rawBackends.isEmpty()) {
        error = QString("No decoder backend was specified in '%1'.").arg(spec);
        return false;
    }
    QSet<QString> seen;
    for (const QString &rawBackend : rawBackends) {
        const QString backend = rawBackend.trimmed().toLower();
        if (!validateDecoderBackend(format, backend)) {
            error = QString("Unsupported %1 decoder backend '%2'.").arg(format, backend);
            return false;
        }
        if (seen.contains(backend)) {
            error = QString("Decoder backend '%1' is repeated in '%2'.").arg(backend, spec);
            return false;
        }
        seen.insert(backend);
        backends.append(backend);
    }
    return true;
}

QStringList decoderCandidatesForEntry(const BenchmarkOptions &options, const QString &entry)
{
    const QString format = imageFormatNameForPath(entry);
    const auto found = options.decoderBackends.constFind(format);
    return found == options.decoderBackends.cend() ? QStringList{"auto"} : found.value();
}

ImageDecodePolicy decodePolicyFor(const QString &format, const QString &backend)
{
    ImageDecodePolicy policy;
    policy.jpeg = JpegDecoderPreference::Auto;
    policy.png = PngDecoderPreference::Auto;
    policy.webp = WebPDecoderPreference::Auto;
    if (format == "jpeg") {
        if (backend == "qt") {
            policy.jpeg = JpegDecoderPreference::Qt;
        } else if (backend == "turbojpeg") {
            policy.jpeg = JpegDecoderPreference::TurboJpeg;
        }
    } else if (format == "png") {
        if (backend == "qt") {
            policy.png = PngDecoderPreference::Qt;
        } else if (backend == "libspng") {
            policy.png = PngDecoderPreference::LibSpng;
        }
    } else if (format == "webp") {
        if (backend == "qt") {
            policy.webp = WebPDecoderPreference::Qt;
        } else if (backend == "libwebp") {
            policy.webp = WebPDecoderPreference::LibWebP;
        }
    }
    return policy;
}

QString requestedDecoderName(const QString &format, const QString &backend)
{
    if (format != "jpeg" && format != "png" && format != "webp") {
        return backend == "auto" ? QString() : backend;
    }
    return QString("%1=%2").arg(format, backend);
}

QStringList rotatedBackends(const QStringList &backends, int iteration)
{
    if (backends.size() <= 1) {
        return backends;
    }
    QStringList result = backends;
    const int offset = iteration % result.size();
    std::rotate(result.begin(), result.begin() + offset, result.end());
    return result;
}

PageResolution resolveSingleImagePage(const PageSelection &selection)
{
    PageResolution resolution;
    if (selection.kind == PageSelectionKind::Index && selection.index != 0) {
        resolution.error = QString("Requested page %1 is out of range for a single image input.")
                               .arg(selection.index);
        return resolution;
    }
    resolution.success = true;
    resolution.index = 0;
    if (selection.kind == PageSelectionKind::First) {
        resolution.source = "first";
    } else if (selection.kind == PageSelectionKind::Index) {
        resolution.source = "numeric";
    } else {
        resolution.source = "fallback-first";
    }
    return resolution;
}

PageResolution resolvePage(const Volume *volume,
                           const PageSelection &selection,
                           const ReadProgressStore::ReadProgressMap &progress)
{
    PageResolution resolution;
    if (!volume || volume->pageCount() <= 0) {
        resolution.error = "The input contains no supported images.";
        return resolution;
    }
    if (selection.kind == PageSelectionKind::First) {
        resolution.success = true;
        resolution.index = 0;
        resolution.source = "first";
        return resolution;
    }
    if (selection.kind == PageSelectionKind::Index) {
        if (selection.index < 0 || selection.index >= volume->pageCount()) {
            resolution.error = QString("Requested page %1 is out of range for %2 pages.")
                                   .arg(selection.index)
                                   .arg(volume->pageCount());
            return resolution;
        }
        resolution.success = true;
        resolution.index = selection.index;
        resolution.source = "numeric";
        return resolution;
    }
    const QString path = QDir::fromNativeSeparators(volume->volumePath());
    const auto saved = progress.constFind(path);
    if (qApp->OpenVolumeWithProgress() && !volume->openedWithSpecifiedImageFile() &&
        saved != progress.cend()) {
        const int index = saved.value().resumePageIndex;
        if (index >= 0 && index < volume->pageCount()) {
            resolution.success = true;
            resolution.index = index;
            resolution.source = "history";
            return resolution;
        }
    }
    resolution.success = true;
    resolution.index = 0;
    resolution.source = "fallback-first";
    return resolution;
}

QString csvValue(QString value)
{
    value.replace('"', "\"\"");
    if (value.contains(',') || value.contains('"') || value.contains('\n') ||
        value.contains('\r')) {
        return QString("\"%1\"").arg(value);
    }
    return value;
}

QString numericCsv(qint64 value)
{
    return value < 0 ? QString() : QString::number(value);
}

QString nanosecondsToMicrosecondsCsv(qint64 nanoseconds)
{
    return nanoseconds < 0 ? QString() : QString::number(nanoseconds / 1000.0, 'f', 3);
}

bool writeCsv(const QString &path, const QVector<BenchmarkRecord> &records)
{
    const QFileInfo info(path);
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
        qCritical().noquote() << "Cannot create benchmark output directory:" << info.absolutePath();
        return false;
    }
    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qCritical().noquote() << "Cannot write benchmark CSV:" << path;
        return false;
    }
    QTextStream stream(&output);
    stream
        << "suite,input,run,success,error,container,archive_size,archive_entry_count,image_count,"
           "selected_entry,selected_uncompressed_size,image_format,width,height,"
           "requested_page,resolved_page,page_source,requested_decoder,actual_decoder,decoder_"
           "fallback_reason,sort";
    for (const ProfileMilestone &milestone : FirstPaintMilestones) {
        stream << ',' << milestone.csvColumn;
    }
    stream << ",library_init_us,archive_open_us,enumeration_us,filter_us,archive_sort_us,page_list_"
              "us,page_sort_us,page_select_us,"
              "source_load_us,extract_us,decode_us,postprocess_us,decode_pipeline_us,total_us\n";
    for (const BenchmarkRecord &record : records) {
        stream << csvValue(record.suite) << ',' << csvValue(record.input) << ',' << record.run
               << ',' << (record.success ? "1" : "0") << ',' << csvValue(record.error) << ','
               << csvValue(record.container) << ',' << numericCsv(record.archiveSize) << ','
               << numericCsv(record.archiveEntryCount) << ',' << numericCsv(record.imageCount)
               << ',' << csvValue(record.entry) << ',' << numericCsv(record.entryUncompressedSize)
               << ',' << csvValue(record.format) << ','
               << (record.sourceSize.isValid() ? QString::number(record.sourceSize.width())
                                               : QString())
               << ','
               << (record.sourceSize.isValid() ? QString::number(record.sourceSize.height())
                                               : QString())
               << ',' << csvValue(record.requestedPage) << ','
               << (record.resolvedPage >= 0 ? QString::number(record.resolvedPage) : QString())
               << ',' << csvValue(record.pageSource) << ',' << csvValue(record.requestedDecoder)
               << ',' << csvValue(record.actualDecoder) << ','
               << csvValue(record.decoderFallbackReason) << ',' << csvValue(record.sort);
        for (const ProfileMilestone &milestone : FirstPaintMilestones) {
            stream << ','
                   << nanosecondsToMicrosecondsCsv(record.profileMilestoneNanoseconds.value(
                          QString::fromLatin1(milestone.label), -1));
        }
        stream << ',' << nanosecondsToMicrosecondsCsv(record.libraryInitNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.archiveOpenNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.enumerationNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.filterNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.archiveSortNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.pageListNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.pageSortNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.pageSelectNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.sourceLoadNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.extractNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.decodeNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.postprocessNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.decodePipelineNanoseconds) << ','
               << nanosecondsToMicrosecondsCsv(record.totalNanoseconds) << '\n';
    }
    return true;
}

double median(QVector<double> values)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const int middle = values.size() / 2;
    if ((values.size() % 2) != 0) {
        return values[middle];
    }
    return (values[middle - 1] + values[middle]) / 2.0;
}

qint64 comparisonMetric(const BenchmarkRecord &record)
{
    if (record.suite == "decode" && record.decodeNanoseconds >= 0) {
        return record.decodeNanoseconds;
    }
    return record.totalNanoseconds;
}

bool writeSummary(const QString &path, const QVector<BenchmarkRecord> &records)
{
    const QFileInfo info(path);
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
        qCritical().noquote() << "Cannot create benchmark summary directory:"
                              << info.absolutePath();
        return false;
    }
    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qCritical().noquote() << "Cannot write benchmark summary:" << path;
        return false;
    }
    QTextStream out(&output);
    out << "Benchmark summary\n";
    struct SummaryGroup
    {
        QString suite;
        QString input;
        QString entry;
        QString requestedDecoder;
        QString actualDecoder;
        QVector<double> totalsMs;
        QVector<double> decodesMs;
        int failures = 0;
    };
    QMap<QString, SummaryGroup> groups;
    for (const BenchmarkRecord &record : records) {
        const QString key = QString("%1\x1f%2\x1f%3\x1f%4\x1f%5")
                                .arg(record.suite,
                                     record.input,
                                     record.entry,
                                     record.requestedDecoder,
                                     record.actualDecoder);
        SummaryGroup &group = groups[key];
        group.suite = record.suite;
        group.input = record.input;
        group.entry = record.entry;
        group.requestedDecoder = record.requestedDecoder;
        group.actualDecoder = record.actualDecoder;
        if (!record.success) {
            ++group.failures;
            continue;
        }
        if (record.totalNanoseconds >= 0) {
            group.totalsMs.append(record.totalNanoseconds / 1000000.0);
        }
        if (record.decodeNanoseconds >= 0) {
            group.decodesMs.append(record.decodeNanoseconds / 1000000.0);
        }
    }
    for (const SummaryGroup &group : groups) {
        out << "  suite=" << group.suite << " input=" << group.input;
        if (!group.entry.isEmpty()) {
            out << " entry=" << group.entry;
        }
        if (!group.requestedDecoder.isEmpty()) {
            out << " requested_decoder=" << group.requestedDecoder;
        }
        if (!group.actualDecoder.isEmpty()) {
            out << " actual_decoder=" << group.actualDecoder;
        }
        out << " runs=" << group.totalsMs.size();
        if (!group.totalsMs.isEmpty()) {
            out << " median_total_ms=" << QString::number(median(group.totalsMs), 'f', 3);
        }
        if (!group.decodesMs.isEmpty()) {
            out << " median_decode_ms=" << QString::number(median(group.decodesMs), 'f', 3);
        }
        if (group.failures > 0) {
            out << " failures=" << group.failures;
        }
        out << '\n';
    }

    QMap<QString, QStringList> decoderOrder;
    QMap<QString, QMap<QString, QVector<double>>> decoderMetrics;
    for (const BenchmarkRecord &record : records) {
        if (!record.success || record.requestedDecoder.isEmpty() ||
            !record.decoderFallbackReason.isEmpty()) {
            continue;
        }
        const qint64 metric = comparisonMetric(record);
        if (metric < 0) {
            continue;
        }
        const QString key = QString("%1\x1f%2\x1f%3").arg(record.suite, record.input, record.entry);
        if (!decoderOrder[key].contains(record.requestedDecoder)) {
            decoderOrder[key].append(record.requestedDecoder);
        }
        decoderMetrics[key][record.requestedDecoder].append(metric / 1000000.0);
    }
    for (auto it = decoderOrder.cbegin(); it != decoderOrder.cend(); ++it) {
        if (it.value().size() < 2) {
            continue;
        }
        const QString baseline = it.value().first();
        const double baselineMedian = median(decoderMetrics[it.key()][baseline]);
        if (baselineMedian <= 0.0) {
            continue;
        }
        for (int i = 1; i < it.value().size(); ++i) {
            const QString candidate = it.value().at(i);
            const double candidateMedian = median(decoderMetrics[it.key()][candidate]);
            if (candidateMedian <= 0.0) {
                continue;
            }
            out << "  comparison baseline=" << baseline << " candidate=" << candidate
                << " speed_ratio=" << QString::number(baselineMedian / candidateMedian, 'f', 3)
                << '\n';
        }
    }
    return true;
}

bool initializeArchiveLibraryIfApplicable(const QString &input, qint64 &elapsedNanoseconds)
{
    elapsedNanoseconds = -1;
    const QFileInfo info(input);
    if (info.isDir() || isDirectImageInput(input) || archiveContainerName(input) == "rar") {
        return true;
    }
    if (FileLoader7zArchive::isInitialized()) {
        elapsedNanoseconds = 0;
        return true;
    }
    QElapsedTimer timer;
    timer.start();
    const bool initialized = FileLoader7zArchive::initializeLib();
    elapsedNanoseconds = timer.nsecsElapsed();
    return initialized;
}

BenchmarkRecord measureDecode(const BenchmarkOptions &options,
                              const QString &input,
                              const QString &entry,
                              const QString &container,
                              const ByteLoader &loader,
                              const QByteArray *preloadedBytes,
                              const QString &backend,
                              int run,
                              int resolvedPage,
                              const QString &pageSource,
                              qint64 archiveSize,
                              qint64 imageCount)
{
    BenchmarkRecord record;
    record.suite = suiteName(options.suite);
    record.input = input;
    record.entry = entry;
    record.container = container;
    record.run = run;
    record.sort = options.sortName;
    record.requestedPage = options.page.requested;
    record.resolvedPage = resolvedPage;
    record.pageSource = pageSource;
    record.archiveSize = archiveSize;
    record.imageCount = imageCount;
    const QString format = imageFormatNameForPath(entry);
    record.format = format;
    record.requestedDecoder = requestedDecoderName(format, backend);
    const ImageDecodePolicy policy = decodePolicyFor(format, backend);

    QByteArray bytes;
    QElapsedTimer totalTimer;
    if (preloadedBytes) {
        bytes = *preloadedBytes;
        totalTimer.start();
    } else {
        totalTimer.start();
        QElapsedTimer sourceTimer;
        sourceTimer.start();
        bytes = loader();
        record.sourceLoadNanoseconds = sourceTimer.nsecsElapsed();
        if (container != "file" && container != "directory") {
            record.extractNanoseconds = record.sourceLoadNanoseconds;
        }
    }
    if (bytes.isEmpty()) {
        record.totalNanoseconds = totalTimer.nsecsElapsed();
        record.error = "Failed to read encoded image bytes.";
        return record;
    }
    record.entryUncompressedSize = bytes.size();
    ImageDecodeMetrics metrics;
    const ImageContent content =
        Volume::decodeImageBytes(entry, bytes, QSize(), QSize(), true, policy, &metrics);
    record.totalNanoseconds = totalTimer.nsecsElapsed();
    record.decodeNanoseconds = metrics.decodeNanoseconds;
    record.decodePipelineNanoseconds = metrics.pipelineNanoseconds;
    record.postprocessNanoseconds =
        qMax<qint64>(0, metrics.pipelineNanoseconds - metrics.decodeNanoseconds);
    record.sourceSize = content.originalSize;
    record.actualDecoder = metrics.decoderBackend;
    record.decoderFallbackReason = fallbackReason(record.requestedDecoder, record.actualDecoder);
    record.success = isDecodedImageValid(content);
    if (!record.success) {
        record.error = "Image decoding failed.";
    }
    return record;
}

void benchmarkSample(const BenchmarkOptions &options,
                     const QString &input,
                     const QString &entry,
                     const QString &container,
                     const ByteLoader &loader,
                     int resolvedPage,
                     const QString &pageSource,
                     qint64 archiveSize,
                     qint64 imageCount,
                     QVector<BenchmarkRecord> &records)
{
    const QStringList backends = decoderCandidatesForEntry(options, entry);
    QByteArray preloaded;
    if (options.suite == BenchmarkSuite::Decode) {
        preloaded = loader();
        if (preloaded.isEmpty()) {
            BenchmarkRecord failure;
            failure.suite = suiteName(options.suite);
            failure.input = input;
            failure.entry = entry;
            failure.container = container;
            failure.error = "Failed to preload encoded image bytes.";
            failure.sort = options.sortName;
            failure.requestedPage = options.page.requested;
            failure.resolvedPage = resolvedPage;
            failure.pageSource = pageSource;
            failure.archiveSize = archiveSize;
            failure.imageCount = imageCount;
            records.append(failure);
            return;
        }
    }
    for (int warmup = 0; warmup < options.warmup; ++warmup) {
        const QStringList order = rotatedBackends(backends, warmup);
        for (const QString &backend : order) {
            measureDecode(options,
                          input,
                          entry,
                          container,
                          loader,
                          options.suite == BenchmarkSuite::Decode ? &preloaded : nullptr,
                          backend,
                          0,
                          resolvedPage,
                          pageSource,
                          archiveSize,
                          imageCount);
        }
    }
    for (int measuredRun = 1; measuredRun <= options.runs; ++measuredRun) {
        const QStringList order = rotatedBackends(backends, measuredRun - 1);
        for (const QString &backend : order) {
            records.append(
                measureDecode(options,
                              input,
                              entry,
                              container,
                              loader,
                              options.suite == BenchmarkSuite::Decode ? &preloaded : nullptr,
                              backend,
                              measuredRun,
                              resolvedPage,
                              pageSource,
                              archiveSize,
                              imageCount));
        }
    }
}

bool benchmarkDecodeInput(const BenchmarkOptions &options,
                          const QString &input,
                          const ReadProgressStore::ReadProgressMap &progress,
                          QVector<BenchmarkRecord> &records)
{
    if (isDirectImageInput(input)) {
        const PageResolution page = resolveSingleImagePage(options.page);
        if (!page.success) {
            qCritical().noquote() << input << ':' << page.error;
            return false;
        }
        const QString path = QFileInfo(input).absoluteFilePath();
        benchmarkSample(
            options,
            input,
            path,
            "file",
            [path] {
                QFile file(path);
                return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
            },
            page.index,
            page.source,
            -1,
            1,
            records);
        return true;
    }

    std::unique_ptr<Volume> volume(VolumeLoader::createVolume(nullptr, input));
    if (!volume) {
        qCritical().noquote() << "Unsupported or unreadable benchmark input:" << input;
        return false;
    }
    volume->loadPageList();
    const PageResolution page = resolvePage(volume.get(), options.page, progress);
    if (!page.success) {
        qCritical().noquote() << input << ':' << page.error;
        return false;
    }
    const QString entryName = volume->pageNameAt(page.index);
    if (entryName.isEmpty()) {
        qCritical().noquote() << "Failed to resolve benchmark page for:" << input;
        return false;
    }
    const QString container = containerNameForInput(input, volume.get());
    const QString decodePath = volume->isArchive() ? entryName : volume->pagePathForName(entryName);
    const qint64 archiveSize = volume->isArchive() ? QFileInfo(volume->volumePath()).size() : -1;
    benchmarkSample(
        options,
        input,
        decodePath,
        container,
        [volumePtr = volume.get(), entryName] { return volumePtr->loadByteArrayByName(entryName); },
        page.index,
        page.source,
        archiveSize,
        volume->pageCount(),
        records);
    return true;
}

BenchmarkRecord measureArchiveOpen(const BenchmarkOptions &options,
                                   const QString &input,
                                   int run,
                                   const ReadProgressStore::ReadProgressMap &progress)
{
    BenchmarkRecord record;
    record.suite = suiteName(options.suite);
    record.input = input;
    record.run = run;
    record.requestedPage = options.page.requested;
    record.sort = options.sortName;
    record.archiveSize = QFileInfo(input).size();
    record.container = archiveContainerName(input);
    QElapsedTimer totalTimer;
    totalTimer.start();
    if (!initializeArchiveLibraryIfApplicable(input, record.libraryInitNanoseconds)) {
        record.totalNanoseconds = totalTimer.nsecsElapsed();
        record.error = "Archive library initialization failed.";
        return record;
    }
    QElapsedTimer openTimer;
    openTimer.start();
    std::unique_ptr<Volume> volume(VolumeLoader::createVolume(nullptr, input));
    record.archiveOpenNanoseconds = openTimer.nsecsElapsed();
    if (!volume || !volume->isArchive()) {
        record.totalNanoseconds = totalTimer.nsecsElapsed();
        record.error = "Input is not a supported archive.";
        return record;
    }
    QElapsedTimer pageListTimer;
    pageListTimer.start();
    volume->loadPageList();
    record.pageListNanoseconds = pageListTimer.nsecsElapsed();
    record.totalNanoseconds = totalTimer.nsecsElapsed();
    record.imageCount = volume->pageCount();
    if (volume->fileLoader()->archiveOpenError() != ArchiveOpenError::None) {
        record.error = "Archive opening or indexing failed.";
        return record;
    }
    const PageResolution page = resolvePage(volume.get(), options.page, progress);
    if (!page.success) {
        record.error = page.error;
        return record;
    }
    record.resolvedPage = page.index;
    record.pageSource = page.source;
    record.success = true;
    return record;
}

bool benchmarkArchiveOpenInput(const BenchmarkOptions &options,
                               const QString &input,
                               const ReadProgressStore::ReadProgressMap &progress,
                               QVector<BenchmarkRecord> &records)
{
    for (int warmup = 0; warmup < options.warmup; ++warmup) {
        measureArchiveOpen(options, input, 0, progress);
    }
    for (int run = 1; run <= options.runs; ++run) {
        records.append(measureArchiveOpen(options, input, run, progress));
    }
    return true;
}

void fillDecodedMetrics(BenchmarkRecord &record,
                        const ImageContent &content,
                        const ImageDecodeMetrics &metrics)
{
    record.decodeNanoseconds = metrics.decodeNanoseconds;
    record.decodePipelineNanoseconds = metrics.pipelineNanoseconds;
    record.postprocessNanoseconds =
        qMax<qint64>(0, metrics.pipelineNanoseconds - metrics.decodeNanoseconds);
    record.sourceSize = content.originalSize;
    record.actualDecoder = metrics.decoderBackend;
    record.decoderFallbackReason = fallbackReason(record.requestedDecoder, record.actualDecoder);
    record.success = isDecodedImageValid(content);
    if (!record.success) {
        record.error = "Selected image decoding failed.";
    }
}

BenchmarkRecord measureDirectFirstImage(const BenchmarkOptions &options,
                                        const QString &input,
                                        const QString &backend,
                                        int run)
{
    BenchmarkRecord record;
    record.suite = suiteName(options.suite);
    record.input = input;
    record.run = run;
    record.requestedPage = options.page.requested;
    record.sort = options.sortName;
    record.container = "file";
    record.imageCount = 1;
    const PageResolution page = resolveSingleImagePage(options.page);
    if (!page.success) {
        record.error = page.error;
        return record;
    }
    record.resolvedPage = page.index;
    record.pageSource = page.source;
    record.entry = QFileInfo(input).absoluteFilePath();
    record.format = imageFormatNameForPath(record.entry);
    record.requestedDecoder = requestedDecoderName(record.format, backend);
    const ImageDecodePolicy policy = decodePolicyFor(record.format, backend);

    QElapsedTimer totalTimer;
    totalTimer.start();
    QElapsedTimer sourceTimer;
    sourceTimer.start();
    QFile file(record.entry);
    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    record.sourceLoadNanoseconds = sourceTimer.nsecsElapsed();
    if (bytes.isEmpty()) {
        record.totalNanoseconds = totalTimer.nsecsElapsed();
        record.error = "Failed to read the image input.";
        return record;
    }
    record.entryUncompressedSize = bytes.size();
    ImageDecodeMetrics metrics;
    const ImageContent content =
        Volume::decodeImageBytes(record.entry, bytes, QSize(), QSize(), true, policy, &metrics);
    record.totalNanoseconds = totalTimer.nsecsElapsed();
    fillDecodedMetrics(record, content, metrics);
    return record;
}

BenchmarkRecord measureVolumeFirstImage(const BenchmarkOptions &options,
                                        const QString &input,
                                        const QString &backend,
                                        int run,
                                        const ReadProgressStore::ReadProgressMap &progress)
{
    BenchmarkRecord record;
    record.suite = suiteName(options.suite);
    record.input = input;
    record.run = run;
    record.requestedPage = options.page.requested;
    record.sort = options.sortName;
    QElapsedTimer totalTimer;
    totalTimer.start();
    if (!initializeArchiveLibraryIfApplicable(input, record.libraryInitNanoseconds)) {
        record.totalNanoseconds = totalTimer.nsecsElapsed();
        record.error = "Archive library initialization failed.";
        return record;
    }
    QElapsedTimer openTimer;
    openTimer.start();
    std::unique_ptr<Volume> volume(VolumeLoader::createVolume(nullptr, input));
    const qint64 openNanoseconds = openTimer.nsecsElapsed();
    if (!volume) {
        record.totalNanoseconds = totalTimer.nsecsElapsed();
        record.error = "Unsupported or unreadable input.";
        return record;
    }
    record.container = containerNameForInput(input, volume.get());
    if (volume->isArchive()) {
        record.archiveSize = QFileInfo(volume->volumePath()).size();
        record.archiveOpenNanoseconds = openNanoseconds;
    }
    QElapsedTimer pageListTimer;
    pageListTimer.start();
    volume->loadPageList();
    record.pageListNanoseconds = pageListTimer.nsecsElapsed();
    record.imageCount = volume->pageCount();

    QElapsedTimer selectTimer;
    selectTimer.start();
    const PageResolution page = resolvePage(volume.get(), options.page, progress);
    record.pageSelectNanoseconds = selectTimer.nsecsElapsed();
    if (!page.success) {
        record.totalNanoseconds = totalTimer.nsecsElapsed();
        record.error = page.error;
        return record;
    }
    record.resolvedPage = page.index;
    record.pageSource = page.source;
    record.entry = volume->pageNameAt(page.index);
    if (record.entry.isEmpty()) {
        record.totalNanoseconds = totalTimer.nsecsElapsed();
        record.error = "Failed to resolve the selected page.";
        return record;
    }
    record.entryUncompressedSize =
        static_cast<qint64>(volume->fileLoader()->getFileSize(record.entry));
    record.format = imageFormatNameForPath(record.entry);
    record.requestedDecoder = requestedDecoderName(record.format, backend);
    const ImageDecodePolicy policy = decodePolicyFor(record.format, backend);

    QElapsedTimer sourceTimer;
    sourceTimer.start();
    const QByteArray bytes = volume->loadByteArrayByName(record.entry);
    record.sourceLoadNanoseconds = sourceTimer.nsecsElapsed();
    if (volume->isArchive()) {
        record.extractNanoseconds = record.sourceLoadNanoseconds;
    }
    if (bytes.isEmpty()) {
        record.totalNanoseconds = totalTimer.nsecsElapsed();
        record.error = "Failed to load the selected image bytes.";
        return record;
    }
    if (record.entryUncompressedSize <= 0) {
        record.entryUncompressedSize = bytes.size();
    }
    ImageDecodeMetrics metrics;
    const ImageContent content =
        Volume::decodeImageBytes(record.entry, bytes, QSize(), QSize(), true, policy, &metrics);
    record.totalNanoseconds = totalTimer.nsecsElapsed();
    fillDecodedMetrics(record, content, metrics);
    return record;
}

QString selectedFirstImageEntry(const BenchmarkOptions &options,
                                const QString &input,
                                const ReadProgressStore::ReadProgressMap &progress,
                                QString &error)
{
    if (isDirectImageInput(input)) {
        const PageResolution page = resolveSingleImagePage(options.page);
        if (!page.success) {
            error = page.error;
            return QString();
        }
        return QFileInfo(input).absoluteFilePath();
    }
    std::unique_ptr<Volume> volume(VolumeLoader::createVolume(nullptr, input));
    if (!volume) {
        error = "Unsupported or unreadable input.";
        return QString();
    }
    volume->loadPageList();
    const PageResolution page = resolvePage(volume.get(), options.page, progress);
    if (!page.success) {
        error = page.error;
        return QString();
    }
    const QString entry = volume->pageNameAt(page.index);
    if (entry.isEmpty()) {
        error = "Failed to resolve the selected page.";
    }
    return entry;
}

bool benchmarkFirstImageInput(const BenchmarkOptions &options,
                              const QString &input,
                              const ReadProgressStore::ReadProgressMap &progress,
                              QVector<BenchmarkRecord> &records)
{
    QString error;
    const QString entry = selectedFirstImageEntry(options, input, progress, error);
    if (entry.isEmpty()) {
        qCritical().noquote() << input << ':' << error;
        return false;
    }
    const QStringList backends = decoderCandidatesForEntry(options, entry);
    const auto measure = [&](const QString &backend, int run) {
        return isDirectImageInput(input)
                   ? measureDirectFirstImage(options, input, backend, run)
                   : measureVolumeFirstImage(options, input, backend, run, progress);
    };
    for (int warmup = 0; warmup < options.warmup; ++warmup) {
        for (const QString &backend : rotatedBackends(backends, warmup)) {
            measure(backend, 0);
        }
    }
    for (int run = 1; run <= options.runs; ++run) {
        for (const QString &backend : rotatedBackends(backends, run - 1)) {
            records.append(measure(backend, run));
        }
    }
    return true;
}

PreparedFirstPaintInput prepareFirstPaintInput(const BenchmarkOptions &options,
                                               const QString &input,
                                               const ReadProgressStore::ReadProgressMap &progress)
{
    PreparedFirstPaintInput prepared;
    if (isDirectImageInput(input)) {
        const PageResolution page = resolveSingleImagePage(options.page);
        if (!page.success) {
            prepared.error = page.error;
            return prepared;
        }
        prepared.childInput = QFileInfo(input).absoluteFilePath();
        prepared.selectedEntry = prepared.childInput;
        prepared.format = imageFormatNameForPath(prepared.selectedEntry);
        prepared.resolvedPage = 0;
        prepared.pageSource = page.source;
        prepared.container = "file";
        prepared.imageCount = 1;
        prepared.entryUncompressedSize = QFileInfo(prepared.childInput).size();
        prepared.success = true;
        return prepared;
    }

    std::unique_ptr<Volume> volume(VolumeLoader::createVolume(nullptr, input));
    if (!volume) {
        prepared.error = "Unsupported or unreadable input.";
        return prepared;
    }
    volume->loadPageList();
    const PageResolution page = resolvePage(volume.get(), options.page, progress);
    if (!page.success) {
        prepared.error = page.error;
        return prepared;
    }
    prepared.selectedEntry = volume->pageNameAt(page.index);
    if (prepared.selectedEntry.isEmpty()) {
        prepared.error = "Failed to resolve the selected page.";
        return prepared;
    }
    prepared.format = imageFormatNameForPath(prepared.selectedEntry);
    prepared.resolvedPage = page.index;
    prepared.pageSource = page.source;
    prepared.container = containerNameForInput(input, volume.get());
    prepared.archiveSize = volume->isArchive() ? QFileInfo(volume->volumePath()).size() : -1;
    prepared.imageCount = volume->pageCount();
    prepared.entryUncompressedSize =
        static_cast<qint64>(volume->fileLoader()->getFileSize(prepared.selectedEntry));
    prepared.childInput = QDir::fromNativeSeparators(volume->volumePath());
    prepared.success = true;
    return prepared;
}

FileSnapshot captureFileSnapshot(const QString &path)
{
    FileSnapshot snapshot;
    snapshot.path = path;
    snapshot.existed = QFileInfo::exists(path);
    if (!snapshot.existed) {
        snapshot.valid = true;
        return snapshot;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return snapshot;
    }
    snapshot.data = file.readAll();
    snapshot.valid = file.error() == QFile::NoError;
    return snapshot;
}

bool restoreFileSnapshot(const FileSnapshot &snapshot)
{
    if (!snapshot.valid) {
        return false;
    }
    if (!snapshot.existed) {
        return !QFileInfo::exists(snapshot.path) || QFile::remove(snapshot.path);
    }
    QSaveFile file(snapshot.path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(snapshot.data) != snapshot.data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

void setDecoderEnvironment(QProcessEnvironment &environment,
                           const QString &format,
                           const QString &backend)
{
    environment.remove(JpegDecoderEnv);
    environment.remove(PngDecoderEnv);
    environment.remove(WebPDecoderEnv);
    if (backend == "auto") {
        return;
    }
    if (format == "jpeg") {
        environment.insert(JpegDecoderEnv, backend);
    } else if (format == "png") {
        environment.insert(PngDecoderEnv, backend);
    } else if (format == "webp") {
        environment.insert(WebPDecoderEnv, backend);
    }
}

QMap<QString, qint64> readProfile(const QString &path)
{
    QMap<QString, qint64> markers;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return markers;
    }
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QStringList columns = stream.readLine().split('\t');
        if (columns.size() < 3) {
            continue;
        }
        bool ok = false;
        const qint64 elapsed = columns.at(0).toLongLong(&ok);
        if (ok) {
            markers[columns.at(2)] = elapsed;
        }
    }
    return markers;
}

qint64 profileDurationNanoseconds(const QMap<QString, qint64> &markers,
                                  const QString &begin,
                                  const QString &end)
{
    if (!markers.contains(begin) || !markers.contains(end)) {
        return -1;
    }
    const qint64 microseconds = markers.value(end) - markers.value(begin);
    return microseconds < 0 ? -1 : microseconds * 1000;
}

BenchmarkRecord measureFirstPaint(const BenchmarkOptions &options,
                                  const QString &input,
                                  const PreparedFirstPaintInput &prepared,
                                  const QString &backend,
                                  int run,
                                  const QString &profilePath)
{
    BenchmarkRecord record;
    record.suite = suiteName(options.suite);
    record.input = input;
    record.run = run;
    record.requestedPage = options.page.requested;
    record.resolvedPage = prepared.resolvedPage;
    record.pageSource = prepared.pageSource;
    record.sort = options.sortName;
    record.entry = prepared.selectedEntry;
    record.format = prepared.format;
    record.container = prepared.container;
    record.archiveSize = prepared.archiveSize;
    record.imageCount = prepared.imageCount;
    record.entryUncompressedSize = prepared.entryUncompressedSize;
    record.requestedDecoder = requestedDecoderName(prepared.format, backend);

    const FileSnapshot settingsSnapshot = captureFileSnapshot(
        qApp->getFilePathOfApplicationSetting(QVApplication::settingsSubPath()));
    const FileSnapshot progressSnapshot = captureFileSnapshot(
        qApp->getFilePathOfApplicationSetting(QVApplication::readProgressSubPath()));
    if (!settingsSnapshot.valid || !progressSnapshot.valid) {
        record.error = "Failed to snapshot QuickViewer settings before the first-paint run.";
        return record;
    }
    const auto restorePersistentState = [&] {
        return restoreFileSnapshot(progressSnapshot) && restoreFileSnapshot(settingsSnapshot);
    };

    QFile::remove(profilePath);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(FirstPaintChildEnv, "1");
    environment.insert(SortEnv, options.sortName);
    environment.insert(RecursiveEnv, options.recursive ? "1" : "0");
    environment.insert(PageIndexEnv, QString::number(prepared.resolvedPage));
    environment.insert("QV_PROFILE_FIRST_IMAGE", QDir::toNativeSeparators(profilePath));
    setDecoderEnvironment(environment, prepared.format, backend);

    QProcess process;
    process.setProgram(QCoreApplication::applicationFilePath());
    process.setArguments({prepared.childInput});
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(30000)) {
        record.error =
            restorePersistentState()
                ? "Failed to start the first-paint child process."
                : "Failed to start the first-paint child process and restore QuickViewer settings.";
        return record;
    }
    if (!process.waitForFinished(120000)) {
        process.kill();
        process.waitForFinished(5000);
        record.error =
            restorePersistentState()
                ? "First-paint child process did not terminate after the first paint."
                : "First-paint child timed out and QuickViewer settings could not be restored.";
        return record;
    }
    if (!restorePersistentState()) {
        record.error =
            "First-paint child completed, but QuickViewer settings could not be restored.";
        return record;
    }

    const QMap<QString, qint64> markers = readProfile(profilePath);
    if (!markers.contains("first-image-painted")) {
        record.error =
            QString("First-paint profile is incomplete (exit code %1).").arg(process.exitCode());
        return record;
    }
    for (const ProfileMilestone &milestone : FirstPaintMilestones) {
        const auto marker = markers.constFind(QString::fromLatin1(milestone.label));
        if (marker != markers.cend()) {
            record.profileMilestoneNanoseconds.insert(QString::fromLatin1(milestone.label),
                                                      marker.value() * 1000);
        }
    }
    record.totalNanoseconds = markers.value("first-image-painted") * 1000;
    if (prepared.container != "file" && prepared.container != "directory") {
        record.archiveOpenNanoseconds =
            profileDurationNanoseconds(markers, "volume-loader.begin", "volume-loader.created");
    }
    record.pageListNanoseconds =
        profileDurationNanoseconds(markers, "volume.page-list.begin", "volume.page-list.end");
    record.extractNanoseconds = profileDurationNanoseconds(
        markers, "image-worker.extract.begin", "image-worker.extract.end");
    record.sourceLoadNanoseconds = record.extractNanoseconds;
    record.decodePipelineNanoseconds = profileDurationNanoseconds(
        markers, "image-worker.extract.end", "image-worker.decode-resize.end");
    record.success = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (!record.success) {
        record.error = QString("First-paint child exited with code %1.").arg(process.exitCode());
    }
    return record;
}

/** Tail of what the child printed, for a failure that needs explaining. */
QString childOutputSummary(QProcess &process)
{
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).simplified();
    if (output.isEmpty()) {
        return QString();
    }
    constexpr int MaximumOutputLength = 300;
    const QString tail = output.right(MaximumOutputLength);
    return QStringLiteral(" Output: %1").arg(tail);
}

/**
 * Runs one empty-window child and reads its profile. The child measures a bare
 * Qt window, so its milestones show how much of a first paint belongs to Qt and
 * Windows rather than to QuickViewer's own startup work.
 */
BenchmarkRecord runEmptyWindowChild(int run, const QString &profilePath)
{
    BenchmarkRecord record;
    record.suite = "empty-window";
    record.run = run;

    QFile::remove(profilePath);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(EmptyWindowChildEnv, "1");
    environment.insert("QV_PROFILE_FIRST_IMAGE", QDir::toNativeSeparators(profilePath));

    QProcess process;
    process.setProgram(QCoreApplication::applicationFilePath());
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(30000)) {
        record.error = "Failed to start the empty-window child process.";
        return record;
    }
    if (!process.waitForFinished(60000)) {
        process.kill();
        process.waitForFinished(5000);
        record.error = "Empty-window child process did not terminate after the first paint.";
        qWarning().noquote() << "empty-window run" << run << ':' << record.error;
        return record;
    }

    const QMap<QString, qint64> markers = readProfile(profilePath);
    if (!markers.contains("first-image-painted")) {
        const QString reason = markers.contains("empty-window.deadline")
                                   ? QStringLiteral("the empty window never painted")
                                   : QStringLiteral("the profile is incomplete");
        record.error = QString("Empty-window child: %1 (exit code %2).%3")
                           .arg(reason)
                           .arg(process.exitCode())
                           .arg(childOutputSummary(process));
        qWarning().noquote() << "empty-window run" << run << ':' << record.error;
        return record;
    }
    for (const ProfileMilestone &milestone : FirstPaintMilestones) {
        const auto marker = markers.constFind(QString::fromLatin1(milestone.label));
        if (marker != markers.cend()) {
            record.profileMilestoneNanoseconds.insert(QString::fromLatin1(milestone.label),
                                                      marker.value() * 1000);
        }
    }
    record.totalNanoseconds = markers.value("first-image-painted") * 1000;
    record.success = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (!record.success) {
        record.error = QString("Empty-window child exited with code %1.").arg(process.exitCode());
        qWarning().noquote() << "empty-window run" << run << ':' << record.error;
    }
    return record;
}

bool benchmarkEmptyWindow(const BenchmarkOptions &options, QVector<BenchmarkRecord> &records)
{
    QTemporaryDir profileDirectory;
    if (!profileDirectory.isValid()) {
        qCritical() << "Cannot create a temporary directory for empty-window profiles.";
        return false;
    }
    for (int warmup = 0; warmup < options.warmup; ++warmup) {
        runEmptyWindowChild(0, profileDirectory.filePath(QString("warmup-%1.tsv").arg(warmup)));
    }
    for (int run = 1; run <= options.runs; ++run) {
        records.append(
            runEmptyWindowChild(run, profileDirectory.filePath(QString("run-%1.tsv").arg(run))));
    }
    return true;
}

bool benchmarkFirstPaintInput(const BenchmarkOptions &options,
                              const QString &input,
                              const ReadProgressStore::ReadProgressMap &progress,
                              QVector<BenchmarkRecord> &records)
{
    const PreparedFirstPaintInput prepared = prepareFirstPaintInput(options, input, progress);
    if (!prepared.success) {
        qCritical().noquote() << input << ':' << prepared.error;
        return false;
    }
    const QStringList backends = decoderCandidatesForEntry(options, prepared.selectedEntry);
    QTemporaryDir profileDirectory;
    if (!profileDirectory.isValid()) {
        qCritical() << "Cannot create a temporary directory for first-paint profiles.";
        return false;
    }
    for (int warmup = 0; warmup < options.warmup; ++warmup) {
        int backendIndex = 0;
        for (const QString &backend : rotatedBackends(backends, warmup)) {
            const QString path = profileDirectory.filePath(
                QString("warmup-%1-%2.tsv").arg(warmup).arg(backendIndex++));
            measureFirstPaint(options, input, prepared, backend, 0, path);
        }
    }
    for (int run = 1; run <= options.runs; ++run) {
        int backendIndex = 0;
        for (const QString &backend : rotatedBackends(backends, run - 1)) {
            const QString path =
                profileDirectory.filePath(QString("run-%1-%2.tsv").arg(run).arg(backendIndex++));
            records.append(measureFirstPaint(options, input, prepared, backend, run, path));
        }
    }
    return true;
}

bool parseOptions(const QStringList &arguments, BenchmarkOptions &options, QString &error)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "QuickViewer benchmark CLI\n\n"
        "Benchmark suites:\n"
        "  decode       Decode and post-processing only; source I/O is excluded.\n"
        "  entry-load   Source read/extraction plus decode and post-processing.\n"
        "  archive-open Archive opening/indexing without image extraction or decode.\n"
        "  first-image  Input opening through the selected decoded image; rendering excluded.\n"
        "  first-paint  Fresh process startup through the first painted image.\n"
        "  empty-window Fresh process startup through the first paint of an empty window.");
    parser.addHelpOption();
    const QCommandLineOption benchmarkOption(
        "benchmark",
        "Benchmark suite: decode, entry-load, archive-open, first-image, first-paint, or "
        "empty-window.",
        "suite");
    const QCommandLineOption runsOption("runs", "Number of measured runs (default: 5).", "N", "5");
    const QCommandLineOption warmupOption(
        "warmup", "Number of unmeasured warmup runs (default: 2).", "N", "2");
    const QCommandLineOption outputOption("output",
                                          "Write raw benchmark records to CSV (default: "
                                          "results/quickviewer-benchmark-<timestamp>.csv).",
                                          "path");
    const QCommandLineOption recursiveOption(
        "recursive", "Recursively include supported images in directory inputs.");
    const QCommandLineOption pageOption("page",
                                        "Initial page: first, resume, or a zero-based page index.",
                                        "first|resume|N",
                                        "first");
    const QCommandLineOption sortOption(
        "sort",
        "Production page sort: name, name-desc, size, size-desc, mtime, or mtime-desc.",
        "mode",
        "name");
    const QCommandLineOption decoderOption(
        "decoder", "Decoder selection; repeatable. FORMAT=BACKEND[,BACKEND...]", "spec");
    parser.addOption(benchmarkOption);
    parser.addOption(runsOption);
    parser.addOption(warmupOption);
    parser.addOption(outputOption);
    parser.addOption(recursiveOption);
    parser.addOption(pageOption);
    parser.addOption(sortOption);
    parser.addOption(decoderOption);
    parser.addPositionalArgument("input", "Input file, directory, or archive.", "[input...]");

    if (!parser.parse(arguments)) {
        error = parser.errorText();
        return false;
    }
    if (parser.isSet("help")) {
        QTextStream(stdout) << parser.helpText();
        options.showHelp = true;
        return true;
    }
    const QString suiteValue = parser.value(benchmarkOption).trimmed().toLower();
    if (suiteValue.isEmpty() || !parseSuite(suiteValue, options.suite)) {
        error = QString("Invalid benchmark suite '%1'.").arg(suiteValue);
        return false;
    }
    if (!parsePositiveInteger(parser.value(runsOption), options.runs)) {
        error = "--runs must be greater than zero.";
        return false;
    }
    if (!parseNonNegativeInteger(parser.value(warmupOption), options.warmup)) {
        error = "--warmup must be zero or greater.";
        return false;
    }
    options.outputPath = parser.value(outputOption);
    if (options.outputPath.isEmpty()) {
        const QString fileName = QString("quickviewer-benchmark-%1.csv")
                                     .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
        options.outputPath = QDir("results").filePath(fileName);
    }
    options.recursive = parser.isSet(recursiveOption);

    const QString pageValue = parser.value(pageOption).trimmed().toLower();
    options.page.requested = pageValue;
    if (pageValue == "first") {
        options.page.kind = PageSelectionKind::First;
    } else if (pageValue == "resume") {
        options.page.kind = PageSelectionKind::Resume;
    } else {
        int pageIndex = 0;
        if (!parseNonNegativeInteger(pageValue, pageIndex)) {
            error =
                QString("Invalid --page value '%1'. Use first, resume, or a non-negative integer.")
                    .arg(pageValue);
            return false;
        }
        options.page.kind = PageSelectionKind::Index;
        options.page.index = pageIndex;
    }
    options.sortName = parser.value(sortOption).trimmed().toLower();
    if (!parseSortMode(options.sortName, options.sort)) {
        error = QString("Invalid --sort value '%1'.").arg(options.sortName);
        return false;
    }
    for (const QString &decoderSpec : parser.values(decoderOption)) {
        QString format;
        QStringList backends;
        QString decoderError;
        if (!parseDecoderSpec(decoderSpec, format, backends, decoderError)) {
            error = decoderError;
            return false;
        }
        if (options.decoderBackends.contains(format)) {
            error = QString("Decoder format '%1' was specified more than once.").arg(format);
            return false;
        }
        options.decoderBackends.insert(format, backends);
    }
    options.inputs = parser.positionalArguments();
    if (options.suite == BenchmarkSuite::EmptyWindow) {
        if (!options.inputs.isEmpty()) {
            error = "empty-window does not take an input.";
            return false;
        }
        return true;
    }
    if (options.inputs.isEmpty()) {
        error = "At least one benchmark input is required.";
        return false;
    }
    if (options.suite == BenchmarkSuite::FirstPaint && options.inputs.size() != 1) {
        error = "first-paint accepts exactly one positional input.";
        return false;
    }
    return true;
}

bool anyFailures(const QVector<BenchmarkRecord> &records)
{
    return std::any_of(records.cbegin(), records.cend(), [](const BenchmarkRecord &record) {
        return !record.success;
    });
}

} // namespace

bool ImageBenchmarkRunner::isRequested(const QStringList &arguments)
{
    for (const QString &argument : arguments) {
        if (argument == "--benchmark" || argument.startsWith("--benchmark=")) {
            return true;
        }
    }
    return false;
}

bool ImageBenchmarkRunner::isEmptyWindowChildRequested()
{
    return !qEnvironmentVariableIsEmpty(EmptyWindowChildEnv);
}

void ImageBenchmarkRunner::applyStartupOverrides()
{
    if (qEnvironmentVariableIsEmpty(FirstPaintChildEnv)) {
        return;
    }
    const bool originalProhibitMultipleRunning = qApp->ProhibitMultipleRunning();
    const bool originalShowSubfolders = qApp->ShowSubfolders();
    const qvEnums::ImageSortBy originalSort = qApp->ImageSortBy();
    const bool originalOpenVolumeWithProgress = qApp->OpenVolumeWithProgress();
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, [=] {
        qApp->setProhibitMultipleRunning(originalProhibitMultipleRunning);
        qApp->setShowSubfolders(originalShowSubfolders);
        qApp->setImageSortBy(originalSort);
        qApp->setOpenVolumeWithProgress(originalOpenVolumeWithProgress);
    });
    qApp->setProhibitMultipleRunning(false);
    qApp->setShowSubfolders(qgetenv(RecursiveEnv) == "1");
    qvEnums::ImageSortBy sort = qvEnums::ImageSortBy::SortByFileName;
    if (parseSortMode(QString::fromLocal8Bit(qgetenv(SortEnv)).toLower(), sort)) {
        qApp->setImageSortBy(sort);
    }
    bool pageOk = false;
    const int pageIndex = QString::fromLocal8Bit(qgetenv(PageIndexEnv)).toInt(&pageOk);
    if (pageOk && pageIndex >= 0 && qApp->readProgressStore() && qApp->arguments().size() >= 2) {
        const QString path = QDir::fromNativeSeparators(qApp->arguments().last());
        qApp->setOpenVolumeWithProgress(true);
        qApp->readProgressStore()->insertSessionOverride(
            path, {QFileInfo(path).fileName(), path, QString(), 0, pageIndex, false});
    }
}

int ImageBenchmarkRunner::run(const QStringList &arguments)
{
    BenchmarkOptions options;
    QString error;
    if (!parseOptions(arguments, options, error)) {
        qCritical().noquote() << error;
        return 2;
    }
    if (options.showHelp) {
        return 0;
    }
    qApp->setImageSortBy(options.sort);
    qApp->setShowSubfolders(options.recursive);
    const ReadProgressStore::ReadProgressMap progress =
        options.page.kind == PageSelectionKind::Resume ? ReadProgressStore::initializeAsync()
                                                       : ReadProgressStore::ReadProgressMap();

    QVector<BenchmarkRecord> records;
    bool inputsSucceeded = true;
    if (options.suite == BenchmarkSuite::EmptyWindow) {
        inputsSucceeded = benchmarkEmptyWindow(options, records);
    }
    for (const QString &input : options.inputs) {
        switch (options.suite) {
        case BenchmarkSuite::Decode:
        case BenchmarkSuite::EntryLoad:
            inputsSucceeded =
                benchmarkDecodeInput(options, input, progress, records) && inputsSucceeded;
            break;
        case BenchmarkSuite::ArchiveOpen:
            inputsSucceeded =
                benchmarkArchiveOpenInput(options, input, progress, records) && inputsSucceeded;
            break;
        case BenchmarkSuite::FirstImage:
            inputsSucceeded =
                benchmarkFirstImageInput(options, input, progress, records) && inputsSucceeded;
            break;
        case BenchmarkSuite::FirstPaint:
            inputsSucceeded =
                benchmarkFirstPaintInput(options, input, progress, records) && inputsSucceeded;
            break;
        case BenchmarkSuite::EmptyWindow:
            break;
        }
    }
    if (!writeCsv(options.outputPath, records)) {
        return 1;
    }
    const QString summaryPath = options.outputPath + ".summary.txt";
    if (!writeSummary(summaryPath, records)) {
        return 1;
    }
    QTextStream(stdout)
        << "CSV: " << QDir::toNativeSeparators(QFileInfo(options.outputPath).absoluteFilePath())
        << '\n'
        << "Summary: " << QDir::toNativeSeparators(QFileInfo(summaryPath).absoluteFilePath())
        << '\n';
    return inputsSucceeded && !anyFailures(records) ? 0 : 1;
}
