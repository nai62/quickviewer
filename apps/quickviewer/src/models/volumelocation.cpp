#include "volumelocation.h"

#include <QDir>
#include <QFileInfo>

#include <utility>

#include "fileloader.h"

OpenTarget OpenTarget::container(QString containerPath)
{
    return {VolumeLocation{std::move(containerPath), QString()}, OpenIntent::Container};
}

OpenTarget OpenTarget::entry(VolumeLocation location)
{
    return {std::move(location), OpenIntent::Entry};
}

OpenTarget OpenTarget::fileInContainer(QString filePath)
{
    const QFileInfo info(QDir::fromNativeSeparators(filePath));
    return {VolumeLocation{info.absolutePath(), info.fileName()}, OpenIntent::FileInContainer};
}

OpenTarget OpenTarget::forPath(QString path)
{
    // Keep the path as the caller spelled it: containers are addressed by the
    // path that was opened, which is what messages and caches have always used.
    const QString normalizedPath = QDir::fromNativeSeparators(path);
    if (IFileLoader::isArchiveFile(normalizedPath)) {
        return container(normalizedPath);
    }
    if (IFileLoader::isImageFile(normalizedPath)) {
        return fileInContainer(normalizedPath);
    }
    return container(normalizedPath);
}

QString volumeLocationDisplayText(const VolumeLocation &location)
{
    const QString containerPath = QDir::toNativeSeparators(location.containerPath);
    if (location.isContainer()) {
        return containerPath;
    }
    if (IFileLoader::isArchiveFile(location.containerPath)) {
        return containerPath + QStringLiteral(" (") + location.entryName + QLatin1Char(')');
    }
    return QDir::toNativeSeparators(
        QDir(location.containerPath).absoluteFilePath(location.entryName));
}
