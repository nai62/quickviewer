#include "volumeloader.h"
#include "fileloaderdirectory.h"
#include "fileloadersubdirectory.h"
#include "fileloader7zarchive.h"
#include "fileloaderrararchive.h"
#include "qvapplication.h"

Volume *VolumeLoader::createVolume(QObject *parent, QString path)
{
    QDir dir(path);

    //    if(dir.exists() && dir.entryList(QDir::Files, QDir::Name).size() > 0) {
    if (dir.exists()) {
        return new Volume(parent, qApp->ShowSubfolders() ? new FileLoaderSubDirectory(parent, path) : new FileLoaderDirectory(parent, path));
    }

    const QFileInfo pathInfo(path);
    const QString completeSuffix = pathInfo.completeSuffix().toLower();

    // Mapping extension aliases to original names
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

    // RAR deploys using unrar directly
    if (archiveFormat == "rar") {
        return new Volume(parent, new FileLoaderRarArchive(parent, path));
    }
    // Automatically recognizes various archive formats that SevenZip can deploy
    if (FileLoader7zArchive::st_supportedArchiveFormats.contains(archiveFormat)) {
        return new Volume(parent, new FileLoader7zArchive(parent, path, archiveFormat, qApp->ExtractSolidArchiveToTemporaryDir()));
    }
    if (IFileLoader::isImageFile(path)) {
        const QFileInfo imageInfo(path);
        const QString directoryPath = imageInfo.absolutePath();
        Volume *volume = new Volume(parent, qApp->ShowSubfolders() ? new FileLoaderSubDirectory(parent, directoryPath) : new FileLoaderDirectory(parent, directoryPath));
        volume->setOpenedWithSpecifiedImageFile(true);
        return volume;
    }
    return nullptr;
}

VolumeLoader::VolumeLoader(QString path)
    : QObject(nullptr),
      m_path(path),
      m_volume(nullptr)
{
}

Volume *VolumeLoader::buildLoadedVolume()
{
    QString volumePath = QDir::toNativeSeparators(m_path);
    if (m_path.contains("::")) {
        const QStringList pathParts = m_path.split("::");
        volumePath = pathParts[0];
    }
    if (!(m_volume = createVolume(nullptr, volumePath))) {
        return m_volume;
    }
    m_volume->moveToThread(QThread::currentThread());
    m_volume->loadPageList();
    if (m_volume->pageCount() == 0) {
        delete m_volume;
        return m_volume = nullptr;
    }
    return m_volume;
}

Volume *VolumeLoader::build()
{
    return buildLoadedVolume();
}

Volume *VolumeLoader::buildForCoverPrefetch()
{
    Volume *volume = buildLoadedVolume();
    if (volume) {
        volume->prefetchCoverImages();
    }
    return volume;
}

Volume *VolumeLoader::buildForCoverPrefetchAsync(QString path)
{
    VolumeLoader volumeLoader(path);
    return volumeLoader.buildForCoverPrefetch();
}

Volume *VolumeLoader::buildForContainingImage()
{
    const QFileInfo imageInfo(QDir::fromNativeSeparators(m_path));
    const QString volumeFolder = imageInfo.absolutePath();
    m_selectedPageName = imageInfo.fileName();
    if (!(m_volume = createVolume(nullptr, volumeFolder))) {
        return m_volume;
    }
    if (m_volume->isArchive()) {
        delete m_volume;
        return m_volume = nullptr;
    }

    // Load the image.
    m_volume->loadPageList();
    const int selectedPageIndex = m_volume->pageIndexForName(m_selectedPageName);
    if (selectedPageIndex < 0) {
        delete m_volume;
        return m_volume = nullptr;
    }
    m_volume->updatePrefetchCache(
        selectedPageIndex, PrefetchMode::Normal, QSize());
    const Volume::ImageLoadFuture initialImageLoad = m_volume->imageLoadAt(selectedPageIndex);
    m_initialImage = initialImageLoad.isValid() ? initialImageLoad.result() : ImageContent();

    return m_volume;
}

ImageContent VolumeLoader::loadThumbnailSourceImage()
{
    if (!(m_volume = createVolume(nullptr, m_path))) {
        return ImageContent();
    }
    ImageContent thumbnailContent = m_volume->loadThumbnailSourceImage();
    delete m_volume;
    return thumbnailContent;
}
