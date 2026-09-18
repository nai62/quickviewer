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
    if (path.contains(QStringLiteral("::"))) {
        return entry(volumeLocationFromString(path));
    }
    const QFileInfo info(QDir::fromNativeSeparators(path));
    if (IFileLoader::isArchiveFile(info.fileName())) {
        return container(info.absoluteFilePath());
    }
    if (IFileLoader::isImageFile(info.fileName())) {
        return fileInContainer(info.absoluteFilePath());
    }
    return container(info.absoluteFilePath());
}

VolumeLocation volumeLocationFromString(const QString &path)
{
    const int separator = path.indexOf(QStringLiteral("::"));
    if (separator < 0) {
        return {QDir::fromNativeSeparators(path), QString()};
    }
    return {QDir::fromNativeSeparators(path.left(separator)), path.mid(separator + 2)};
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
