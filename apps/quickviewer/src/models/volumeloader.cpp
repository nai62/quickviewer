#include "volumeloader.h"
#include "fileloaderdirectory.h"
#include "fileloadersubdirectory.h"
#include "fileloader7zarchive.h"
#include "fileloaderrararchive.h"
#include "qvapplication.h"
#include "startupprofiler.h"

#include <memory>
#include <utility>

static VolumeBuildResult volumeFromLoader(QObject *parent, std::unique_ptr<IFileLoader> loader)
{
    if (!loader) {
        return {};
    }
    ArchiveOpenError error = loader->archiveOpenError();
    if (!loader->isValid()) {
        if (error == ArchiveOpenError::None) {
            error = ArchiveOpenError::Corrupt;
        }
        return {nullptr, error};
    }
    return {new Volume(parent, std::move(loader)), ArchiveOpenError::None};
}

VolumeBuildResult VolumeLoader::createVolumeResult(QObject *parent, QString path)
{
    QDir dir(path);
    if (dir.exists()) {
        std::unique_ptr<IFileLoader> loader;
        if (qApp->ShowSubfolders()) {
            loader = std::make_unique<FileLoaderSubDirectory>(path);
        } else {
            loader = std::make_unique<FileLoaderDirectory>(path);
        }
        return volumeFromLoader(parent, std::move(loader));
    }

    const QFileInfo pathInfo(path);
    const QString completeSuffix = pathInfo.completeSuffix().toLower();
    QString archiveFormat = pathInfo.suffix().toLower();
    if (completeSuffix.right(3) == "cbz") {
        archiveFormat = "zip";
    }
    if (completeSuffix.right(3) == "cbr") {
        archiveFormat = "rar";
    }
    if (completeSuffix.right(3) == "cb7") {
        archiveFormat = "7z";
    }
    if (completeSuffix.right(6) == "tar.gz") {
        archiveFormat = "tgz";
    }
    if (completeSuffix.right(7) == "tar.bz2") {
        archiveFormat = "tbz2";
    }
    if (completeSuffix.right(6) == "tar.xz") {
        archiveFormat = "txz";
    }

    if (archiveFormat == "rar") {
        return volumeFromLoader(parent, std::make_unique<FileLoaderRarArchive>(path));
    }

    const bool lib7zipReady = FileLoader7zArchive::initializeLib();
    if (lib7zipReady && FileLoader7zArchive::st_supportedArchiveFormats.contains(archiveFormat)) {
        return volumeFromLoader(
            parent,
            std::make_unique<FileLoader7zArchive>(
                path, archiveFormat, qApp->ExtractSolidArchiveToTemporaryDir()));
    }

    if (IFileLoader::isImageFile(path)) {
        const QFileInfo imageInfo(path);
        const QString directoryPath = imageInfo.absolutePath();
        std::unique_ptr<IFileLoader> loader;
        if (qApp->ShowSubfolders()) {
            loader = std::make_unique<FileLoaderSubDirectory>(directoryPath);
        } else {
            loader = std::make_unique<FileLoaderDirectory>(directoryPath);
        }
        VolumeBuildResult result = volumeFromLoader(parent, std::move(loader));
        if (result.volume) {
            result.volume->setOpenedWithSpecifiedImageFile(true);
        }
        return result;
    }

    if (IFileLoader::isArchiveFile(path)) {
        return {nullptr, ArchiveOpenError::Unsupported};
    }
    return {};
}

Volume *VolumeLoader::createVolume(QObject *parent, QString path)
{
    return createVolumeResult(parent, std::move(path)).volume;
}

VolumeLoader::VolumeLoader(QString path)
    : QObject(nullptr),
      m_path(std::move(path)),
      m_volume(nullptr)
{
}

VolumeBuildResult VolumeLoader::buildLoadedVolume()
{
    StartupProfiler::mark("volume-loader.begin");
    // Callers address a container with a real filesystem path; entries inside a
    // container are resolved before a loader is created.
    const QString volumePath = QDir::toNativeSeparators(m_path);
    VolumeBuildResult result = createVolumeResult(nullptr, volumePath);
    m_volume = result.volume;
    if (!m_volume) {
        return result;
    }

    StartupProfiler::mark("volume-loader.created");
    m_volume->moveToThread(QThread::currentThread());
    m_volume->loadPageList();
    StartupProfiler::mark("volume-loader.page-list-loaded");

    const ArchiveOpenError loadError = m_volume->fileLoader()
                                           ? m_volume->fileLoader()->archiveOpenError()
                                           : ArchiveOpenError::None;
    if (loadError != ArchiveOpenError::None) {
        delete m_volume;
        m_volume = nullptr;
        return {nullptr, loadError};
    }
    if (m_volume->pageCount() == 0) {
        delete m_volume;
        m_volume = nullptr;
        return {};
    }
    return {m_volume, ArchiveOpenError::None};
}

VolumeBuildResult VolumeLoader::buildResult()
{
    return buildLoadedVolume();
}

Volume *VolumeLoader::build()
{
    return buildResult().volume;
}

VolumeBuildResult VolumeLoader::buildForCoverPrefetchResult()
{
    VolumeBuildResult result = buildLoadedVolume();
    if (!result.volume) {
        return result;
    }

    result.volume->prefetchCoverImages();
    const int coverCount = qMin(2, result.volume->pageCount());
    for (int pageIndex = 0; pageIndex < coverCount; ++pageIndex) {
        const Volume::ImageLoadFuture load = result.volume->imageLoadAt(pageIndex);
        if (load.isValid()) {
            load.result();
        }
        const ArchiveOpenError error = result.volume->fileLoader()
                                           ? result.volume->fileLoader()->archiveOpenError()
                                           : ArchiveOpenError::None;
        if (error != ArchiveOpenError::None) {
            delete result.volume;
            result.volume = nullptr;
            result.error = error;
            return result;
        }
    }
    return result;
}

Volume *VolumeLoader::buildForCoverPrefetch()
{
    return buildForCoverPrefetchResult().volume;
}

Volume *VolumeLoader::buildForCoverPrefetchAsync(QString path)
{
    VolumeLoader volumeLoader(std::move(path));
    return volumeLoader.buildForCoverPrefetch();
}

Volume *VolumeLoader::buildForContainingImage()
{
    const QFileInfo imageInfo(QDir::fromNativeSeparators(m_path));
    const QString volumeFolder = imageInfo.absolutePath();
    m_selectedPageName = imageInfo.fileName();
    VolumeBuildResult result = createVolumeResult(nullptr, volumeFolder);
    m_volume = result.volume;
    if (!m_volume) {
        return nullptr;
    }
    if (m_volume->isArchive()) {
        delete m_volume;
        return m_volume = nullptr;
    }

    m_volume->loadPageList();
    const int selectedPageIndex = m_volume->pageIndexForName(m_selectedPageName);
    if (selectedPageIndex < 0) {
        delete m_volume;
        return m_volume = nullptr;
    }
    m_volume->updatePrefetchCache(selectedPageIndex, PrefetchMode::Normal, QSize());
    const Volume::ImageLoadFuture initialImageLoad = m_volume->imageLoadAt(selectedPageIndex);
    m_initialImage = initialImageLoad.isValid() ? initialImageLoad.result() : ImageContent();

    return m_volume;
}

ImageContent VolumeLoader::loadThumbnailSourceImage()
{
    VolumeBuildResult result = createVolumeResult(nullptr, m_path);
    m_volume = result.volume;
    if (!m_volume) {
        return ImageContent();
    }
    ImageContent thumbnailContent = m_volume->loadThumbnailSourceImage();
    delete m_volume;
    m_volume = nullptr;
    return thumbnailContent;
}
