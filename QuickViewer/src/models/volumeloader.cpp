#include "volumeloader.h"
#include "fileloaderdirectory.h"
#include "fileloadersubdirectory.h"
#include "fileloader7zarchive.h"
#include "fileloaderrararchive.h"
#include "qvapplication.h"

Volume *VolumeLoader::CreateVolume(QObject *parent, QString path)
{
    QDir dir(path);

    //    if(dir.exists() && dir.entryList(QDir::Files, QDir::Name).size() > 0) {
    if (dir.exists()) {
        return new Volume(parent, qApp->ShowSubfolders() ? new FileLoaderSubDirectory(parent, path) : new FileLoaderDirectory(parent, path));
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
        return new Volume(parent, new FileLoaderRarArchive(parent, path));
    }
    // Automatically recognizes various archive formats that SevenZip can deploy
    if (FileLoader7zArchive::st_supportedArchiveFormats.contains(fmt)) {
        return new Volume(parent, new FileLoader7zArchive(parent, path, fmt, qApp->ExtractSolidArchiveToTemporaryDir()));
    }
    if (IFileLoader::isImageFile(path)) {
        const QFileInfo imageInfo(path);
        const QString dirpath = imageInfo.absolutePath();
        Volume *fvd = new Volume(parent, qApp->ShowSubfolders() ? new FileLoaderSubDirectory(parent, dirpath) : new FileLoaderDirectory(parent, dirpath));
        fvd->selectPageByName(imageInfo.fileName());
        fvd->setOpenedWithSpecifiedImageFile(true);
        return fvd;
    }
    return nullptr;
}

VolumeLoader::VolumeLoader(QString path)
    : QObject(nullptr),
      Path(path),
      m_volume(nullptr)
{
}

Volume *VolumeLoader::build(bool onlyCover)
{
    QString pathbase = QDir::toNativeSeparators(Path);
    QString subfilename;
    if (Path.contains("::")) {
        QStringList seps = Path.split("::");
        pathbase = seps[0];
        subfilename = seps[1];
    }
    if (!(m_volume = CreateVolume(nullptr, pathbase))) {
        return m_volume;
    }
    m_volume->moveToThread(QThread::currentThread());
    m_volume->enumerate();
    if (m_volume->pageCount() == 0) {
        delete m_volume;
        return m_volume = nullptr;
    }
    Volume::CacheMode mode = onlyCover ? Volume::CoverOnly : Volume::Normal;
    m_volume->setCacheMode(mode);
    if (Filenames.isEmpty()) {
        checkBookProgress();
    } else if (subfilename.length() > 0) {
        m_volume->selectPageByName(subfilename);
    }
    m_volume->handleReady();
    return m_volume;
}

Volume *VolumeLoader::buildAsync(QString path, bool onlyCover)
{
    VolumeLoader builder(path);
    return builder.build(onlyCover);
}

Volume *VolumeLoader::buildForAssoc()
{
    const QFileInfo imageInfo(QDir::fromNativeSeparators(Path));
    const QString volumeFolder = imageInfo.absolutePath();
    m_subfilename = imageInfo.fileName();
    if (!(m_volume = CreateVolume(nullptr, volumeFolder))) {
        return m_volume;
    }
    if (m_volume->isArchive()) {
        delete m_volume;
        return m_volume = nullptr;
    }

    // Load the image.
    m_volume->enumerate();
    if (!m_volume->selectPageByName(m_subfilename)) {
        delete m_volume;
        return m_volume = nullptr;
    }
    Ic = m_volume->currentImage();

    return m_volume;
}

ImageContent VolumeLoader::thumbnail()
{
    if (!(m_volume = CreateVolume(nullptr, Path))) {
        return ImageContent();
    }
    checkBookProgress();
    m_volume->setCacheMode(Volume::CreateThumbnail);
    m_volume->handleReady();
    auto ic = m_volume->currentImage();
    delete m_volume;
    return ic;
}

void VolumeLoader::checkBookProgress()
{
    // change page by progress.ini
    QString volumepath = QDir::fromNativeSeparators(m_volume->volumePath());
    if (qApp->OpenVolumeWithProgress() && !m_volume->openedWithSpecifiedImageFile() && qApp->bookshelfManager()->contains(volumepath)) {
        BookProgress book = qApp->bookshelfManager()->at(volumepath);
        m_volume->selectPage(book.Current);
    }
    m_volume->moveToThread(QThread::currentThread());
}
