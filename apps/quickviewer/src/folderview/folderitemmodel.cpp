#include "folderitemmodel.h"
#include "startupprofiler.h"

#ifdef Q_OS_WIN
#    include <windows.h>
#    include <shellapi.h>
#endif

namespace {
/**
 * True when the UI font itself has a glyph for the character. Qt checks the
 * whole fallback chain when it lays text out, and loading the font it picks for
 * a missing glyph is what costs hundreds of milliseconds.
 */
bool primaryFontSupports(char32_t code)
{
    static const QRawFont raw = QRawFont::fromFont(QApplication::font());
    return raw.isValid() && raw.supportsCharacter(code);
}

/**
 * The character that stands in for a glyph the UI font cannot draw.
 *
 * '□' is what the platform itself draws for a missing glyph and reads as "no
 * glyph here" in every language, but no character is guaranteed to exist in a
 * font: of the twenty UI fonts this was measured against, one lacks it. The
 * middle dot and '?' are wider spread, so they are the fallbacks, and asking for
 * them is the cheap side of the check that keeps the placeholder from needing a
 * fallback font of its own.
 */
QChar placeholderCharacter()
{
    static const QChar chosen = [] {
        const QRawFont raw = QRawFont::fromFont(QApplication::font());
        const char32_t candidates[] = {0x25A1, 0x00B7, U'?'};
        for (const char32_t candidate : candidates) {
            if (raw.isValid() && raw.supportsCharacter(candidate)) {
                return QChar(candidate);
            }
        }
        return QChar(QLatin1Char('?'));
    }();
    return chosen;
}

/**
 * Glyphs whose fallback font this process has already loaded. Loading one costs
 * hundreds of milliseconds, so a list keeps using a placeholder only for glyphs
 * it has not loaded yet; a later list that holds the same characters is drawn
 * with the real names straight away.
 */
QSet<char32_t> &warmedFallbackGlyphs()
{
    static QSet<char32_t> glyphs;
    return glyphs;
}

/** Glyphs the running fallback-font load covers. */
QSet<char32_t> &fallbackLoadInFlight()
{
    static QSet<char32_t> glyphs;
    return glyphs;
}

/**
 * Replaces every character the UI font cannot draw and that no fallback font has
 * been loaded for with the placeholder, or returns an empty string when the name
 * can be drawn as it is. The glyphs that would still need a load are reported
 * through \a glyphsToLoad.
 */
QString placeholderName(const QString &name, QSet<char32_t> *glyphsToLoad)
{
    QString placeholder;
    bool replaced = false;
    for (int index = 0; index < name.size(); ++index) {
        const QChar character = name.at(index);
        if (character.unicode() < 0x80) {
            placeholder.append(character);
            continue;
        }
        char32_t code = character.unicode();
        int length = 1;
        if (character.isHighSurrogate() && index + 1 < name.size() &&
            name.at(index + 1).isLowSurrogate()) {
            code = QChar::surrogateToUcs4(character, name.at(index + 1));
            length = 2;
        }
        if (primaryFontSupports(code) || warmedFallbackGlyphs().contains(code)) {
            placeholder.append(name.mid(index, length));
            index += length - 1;
            continue;
        }
        if (glyphsToLoad) {
            glyphsToLoad->insert(code);
        }
        placeholder.append(placeholderCharacter());
        replaced = true;
        index += length - 1;
    }
    return replaced ? placeholder : QString();
}

#ifdef Q_OS_WIN
QImage shellIconImage(const wchar_t *path, DWORD attributes)
{
    SHFILEINFOW info = {};
    if (!SHGetFileInfoW(path,
                        attributes,
                        &info,
                        sizeof(info),
                        SHGFI_ICON | SHGFI_SHELLICONSIZE | SHGFI_USEFILEATTRIBUTES)) {
        return QImage();
    }
    const QImage image = QImage::fromHICON(info.hIcon);
    DestroyIcon(info.hIcon);
    return image;
}

FolderIconImages loadShellIcons()
{
    // The shell asks for an initialized COM apartment on the calling thread.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    FolderIconImages images;
    images.folder = shellIconImage(L"C:\\folder", FILE_ATTRIBUTE_DIRECTORY);
    images.archive = shellIconImage(L"archive.zip", FILE_ATTRIBUTE_NORMAL);
    images.image = shellIconImage(L"image.png", FILE_ATTRIBUTE_NORMAL);
    if (SUCCEEDED(com)) {
        CoUninitialize();
    }
    return images;
}

QFuture<FolderIconImages> &iconLoadFuture()
{
    static QFuture<FolderIconImages> future;
    if (!future.isValid()) {
        future = QtConcurrent::run(loadShellIcons);
    }
    return future;
}
#endif
} // namespace

void FolderItemModel::startIconLoad()
{
#ifdef Q_OS_WIN
    (void)iconLoadFuture();
#endif
}

FolderItemModel::FolderItemModel(QObject *parent)
    : QAbstractItemModel(parent),
      m_searchedVolumes(nullptr),
      m_currentVolumeRow(-1)
{
#ifdef Q_OS_WIN
    // The shell renders the icons on another thread; this thread only turns the
    // finished images into pixmaps. Until they arrive the list draws without
    // them, which is why the load starts while the window is still being built.
    connect(&m_iconWatcher,
            &QFutureWatcher<FolderIconImages>::finished,
            this,
            &FolderItemModel::handleIconLoadFinished);
    connect(&m_fallbackFontWatcher,
            &QFutureWatcher<void>::finished,
            this,
            &FolderItemModel::handleFallbackFontsLoaded);
    const QFuture<FolderIconImages> &future = iconLoadFuture();
    if (future.isFinished()) {
        applyIconImages(future.result());
    } else {
        m_iconWatcher.setFuture(future);
    }
#else
    loadIconsFromProvider();
#endif
}

void FolderItemModel::handleIconLoadFinished()
{
    applyIconImages(m_iconWatcher.future().result());
}

void FolderItemModel::applyIconImages(const FolderIconImages &images)
{
    StartupProfiler::mark("folder-item-icons.begin");
    if (images.folder.isNull() || images.archive.isNull() || images.image.isNull()) {
        // The shell did not answer for every icon (another platform, or a
        // refusal): fall back to the provider, which is what this used to do.
        loadIconsFromProvider();
        return;
    }
    m_folderIcon = QIcon(QPixmap::fromImage(images.folder));
    m_archiveIcon = QIcon(QPixmap::fromImage(images.archive));
    m_imageIcon = QIcon(QPixmap::fromImage(images.image));
    StartupProfiler::mark("folder-item-icons.end");
    if (rowCount(QModelIndex()) > 0) {
        emit dataChanged(index(0, 0), index(rowCount(QModelIndex()) - 1, 0), {Qt::DecorationRole});
    }
}

void FolderItemModel::loadIconsFromProvider()
{
    QFileIconProvider iconProvider;
    m_folderIcon = iconProvider.icon(QFileIconProvider::Folder);
    m_archiveIcon = iconProvider.icon(QFileInfo(QStringLiteral("archive.zip")));
    m_imageIcon = iconProvider.icon(QFileInfo(QStringLiteral("image.png")));

    const QIcon fileIcon = iconProvider.icon(QFileIconProvider::File);
    if (m_folderIcon.isNull()) {
        m_folderIcon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
    }
    if (m_archiveIcon.isNull()) {
        m_archiveIcon = fileIcon;
    }
    if (m_imageIcon.isNull()) {
        m_imageIcon = fileIcon;
    }
    StartupProfiler::mark("folder-item-icons.end");
    if (rowCount(QModelIndex()) > 0) {
        emit dataChanged(index(0, 0), index(rowCount(QModelIndex()) - 1, 0), {Qt::DecorationRole});
    }
}

QVariant FolderItemModel::data(const QModelIndex &index, int role) const
{
    if (!m_searchedVolumes) {
        return QVariant();
    }
    const int row = index.row();
    const FolderItem &fi = m_searchedVolumes->at(row);
    switch (role) {
    case Qt::DisplayRole:
        if (m_placeholdersActive && row < m_placeholderNames.size() &&
            !m_placeholderNames.at(row).isEmpty()) {
            return m_placeholderNames.at(row);
        }
        return fi.name;
    case Qt::DecorationRole:
        switch (fi.type) {
        case FolderItem::Dir:
            return m_folderIcon;
        case FolderItem::Archive:
            return m_archiveIcon;
        case FolderItem::Image:
            return m_imageIcon;
        case FolderItem::NoItems:
            break;
        }
        break;
    case CurrentVolumeRole:
        return row == m_currentVolumeRow;
    }
    return QVariant();
}

int FolderItemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return !m_searchedVolumes ? 0 : m_searchedVolumes->size();
}

int FolderItemModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QModelIndex FolderItemModel::index(int row, int column, const QModelIndex &) const
{
    if (!m_searchedVolumes) {
        return QModelIndex();
    }
    return row < m_searchedVolumes->size()
               ? createIndex(row, column, (void *)&m_searchedVolumes->at(row))
               : QModelIndex();
}

QModelIndex FolderItemModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

void FolderItemModel::setVolumes(QList<FolderItem> *volumes)
{
    if (!volumes) {
        return;
    }
    emit beginResetModel();
    m_searchedVolumes = volumes;
    updatePlaceholderNames();
    emit endResetModel();
}

void FolderItemModel::updatePlaceholderNames()
{
    m_placeholderNames.clear();
    m_glyphsToLoad.clear();
    m_placeholdersActive = false;
    if (!m_searchedVolumes) {
        return;
    }
    for (const FolderItem &item : *m_searchedVolumes) {
        const QString placeholder = placeholderName(item.name, &m_glyphsToLoad);
        m_placeholderNames.append(placeholder);
        if (!placeholder.isEmpty()) {
            m_placeholdersActive = true;
        }
    }
}

QStringList FolderItemModel::namesNeedingFallback() const
{
    QStringList names;
    if (!m_searchedVolumes) {
        return names;
    }
    for (int row = 0; row < m_placeholderNames.size() && row < m_searchedVolumes->size(); ++row) {
        if (!m_placeholderNames.at(row).isEmpty()) {
            names.append(m_searchedVolumes->at(row).name);
        }
    }
    return names;
}

void FolderItemModel::requestFallbackFonts()
{
    if (m_glyphsToLoad.isEmpty() || !fallbackLoadInFlight().isEmpty()) {
        return;
    }
    const QStringList names = namesNeedingFallback();
    if (names.isEmpty()) {
        return;
    }
    fallbackLoadInFlight() = m_glyphsToLoad;
    const QFont listFont = QApplication::font();
    m_fallbackFontWatcher.setFuture(QtConcurrent::run([names, listFont] {
        // Shaping the names is what loads the fallback fonts; the load is cached
        // per process and shared with the GUI thread, which therefore never
        // blocks on it.
        QImage scratch(4096, 128, QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&scratch);
        painter.setFont(listFont);
        for (const QString &name : names) {
            painter.drawText(QPoint(0, 64), name);
        }
    }));
}

void FolderItemModel::handleFallbackFontsLoaded()
{
    warmedFallbackGlyphs().unite(fallbackLoadInFlight());
    fallbackLoadInFlight().clear();
    updatePlaceholderNames();
    if (rowCount(QModelIndex()) > 0) {
        emit dataChanged(index(0, 0), index(rowCount(QModelIndex()) - 1, 0), {Qt::DisplayRole});
    }
    // A list that arrived while this load was running can still hold glyphs of
    // its own.
    requestFallbackFonts();
}

void FolderItemModel::setCurrentVolumeRow(int row)
{
    if (!m_searchedVolumes || row < 0 || row >= m_searchedVolumes->size()) {
        row = -1;
    }
    if (row == m_currentVolumeRow) {
        return;
    }

    const int previousRow = m_currentVolumeRow;
    m_currentVolumeRow = row;
    if (previousRow >= 0) {
        emit dataChanged(index(previousRow, 0), index(previousRow, 0), {CurrentVolumeRole});
    }
    if (m_currentVolumeRow >= 0) {
        emit dataChanged(
            index(m_currentVolumeRow, 0), index(m_currentVolumeRow, 0), {CurrentVolumeRole});
    }
}
