#include "volumemanagerbuilder.h"
#include "fileloaderdirectory.h"
#include "fileloadersubdirectory.h"
#include "fileloader7zarchive.h"
#include "fileloaderrararchive.h"
#include "qvapplication.h"

VolumeManager *VolumeManagerBuilder::CreateVolume(QObject *parent, QString path)
{
    QDir dir(path);

    //    if(dir.exists() && dir.entryList(QDir::Files, QDir::Name).size() > 0) {
    if (dir.exists()) {
        return new VolumeManager(parent, qApp->ShowSubfolders() ? new FileLoaderSubDirectory(parent, path) : new FileLoaderDirectory(parent, path));
    }

    QFileInfo pathinfo(path);
    QString ext = pathinfo.completeSuffix().toLower();

    // Mapping extension aliases to original names
    QString fmt = pathinfo.suffix().toLower();
    if (ext.right(3) == "cbz") {
        fmt = "zip";
    }
    if (ext.right(3) == "cbr") {
        fmt = "rar";
    }
    if (ext.right(3) == "cb7") {
        fmt = "7z";
    }
    if (ext.right(6) == "tar.gz") {
        fmt = "tgz";
    }
    if (ext.right(7) == "tar.bz2") {
        fmt = "tbz2";
    }
    if (ext.right(6) == "tar.xz") {
        fmt = "txz";
    }

    // RAR deploys using unrar directly
    if (fmt == "rar") {
        return new VolumeManager(parent, new FileLoaderRarArchive(parent, path));
    }
    // Automatically recognizes various archive formats that SevenZip can deploy
    if (FileLoader7zArchive::st_supportedArchiveFormats.contains(fmt)) {
        return new VolumeManager(parent, new FileLoader7zArchive(parent, path, fmt, qApp->ExtractSolidArchiveToTemporaryDir()));
    }
    if (IFileLoader::isImageFile(path)) {
        const QFileInfo imageInfo(path);
        const QString dirpath = imageInfo.absolutePath();
        VolumeManager *fvd = new VolumeManager(parent, qApp->ShowSubfolders() ? new FileLoaderSubDirectory(parent, dirpath) : new FileLoaderDirectory(parent, dirpath));
        fvd->findImageByName(imageInfo.fileName());
        fvd->setOpenedWithSpecifiedImageFile(true);
        return fvd;
    }
    return nullptr;
}

VolumeManagerBuilder::VolumeManagerBuilder(QString path)
    : QObject(nullptr),
      Path(path),
      m_volumeManager(nullptr)
{
}

VolumeManager *VolumeManagerBuilder::build(bool onlyCover)
{
    QString pathbase = QDir::toNativeSeparators(Path);
    QString subfilename;
    if (Path.contains("::")) {
        QStringList seps = Path.split("::");
        pathbase = seps[0];
        subfilename = seps[1];
    }
    if (!(m_volumeManager = CreateVolume(nullptr, pathbase))) {
        return m_volumeManager;
    }
    m_volumeManager->moveToThread(QThread::currentThread());
    m_volumeManager->enumerate();
    if (m_volumeManager->size() == 0) {
        delete m_volumeManager;
        return m_volumeManager = nullptr;
    }
    VolumeManager::CacheMode mode = onlyCover ? VolumeManager::CoverOnly : VolumeManager::Normal;
    m_volumeManager->setCacheMode(mode);
    if (Filenames.isEmpty()) {
        checkBookProgress();
    } else if (subfilename.length() > 0) {
        m_volumeManager->findImageByName(subfilename);
    }
    m_volumeManager->handleReady();
    return m_volumeManager;
}

VolumeManager *VolumeManagerBuilder::buildAsync(QString path, bool onlyCover)
{
    VolumeManagerBuilder builder(path);
    return builder.build(onlyCover);
}

VolumeManager *VolumeManagerBuilder::buildForAssoc()
{
    const QFileInfo imageInfo(QDir::fromNativeSeparators(Path));
    const QString volumeFolder = imageInfo.absolutePath();
    m_subfilename = imageInfo.fileName();
    if (!(m_volumeManager = CreateVolume(nullptr, volumeFolder))) {
        return m_volumeManager;
    }
    if (m_volumeManager->isArchive()) {
        delete m_volumeManager;
        return m_volumeManager = nullptr;
    }

    // Load the image.
    m_volumeManager->enumerate();
    if (!m_volumeManager->findImageByName(m_subfilename)) {
        delete m_volumeManager;
        return m_volumeManager = nullptr;
    }
    Ic = m_volumeManager->currentImage();

    return m_volumeManager;
}

ImageContent VolumeManagerBuilder::thumbnail()
{
    if (!(m_volumeManager = CreateVolume(nullptr, Path))) {
        return ImageContent();
    }
    checkBookProgress();
    m_volumeManager->setCacheMode(VolumeManager::CreateThumbnail);
    m_volumeManager->handleReady();
    auto ic = m_volumeManager->currentImage();
    delete m_volumeManager;
    return ic;
}

void VolumeManagerBuilder::checkBookProgress()
{
    // change page by progress.ini
    QString volumepath = QDir::fromNativeSeparators(m_volumeManager->volumePath());
    if (qApp->OpenVolumeWithProgress() && !m_volumeManager->openedWithSpecifiedImageFile() && qApp->bookshelfManager()->contains(volumepath)) {
        BookProgress book = qApp->bookshelfManager()->at(volumepath);
        m_volumeManager->findPageByIndex(book.Current);
    }
    m_volumeManager->moveToThread(QThread::currentThread());
}
