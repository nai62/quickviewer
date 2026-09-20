#ifndef FOLDERITEMMODEL_H
#define FOLDERITEMMODEL_H

#include <QtWidgets>
#include <QtCore>

#include "folderitem.h"

/**
 * The three list icons as the shell renders them. Loaded off the GUI thread
 * because resolving them costs the calling thread tens of milliseconds.
 */
struct FolderIconImages
{
    QImage folder;
    QImage archive;
    QImage image;
};

class FolderItemModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum ItemRole { CurrentVolumeRole = Qt::UserRole };

    FolderItemModel(QObject *parent);
    /**
     * Starts loading the list icons in the background. Called while the window
     * is still being created, so the panel does not have to wait for the shell
     * when it appears.
     */
    static void startIconLoad();
    /**
     * Names, in display order, that need a fallback font. The rows without one
     * are empty entries.
     */
    QStringList namesNeedingFallback() const;
    /**
     * Loads the fallback fonts the current list needs, on a worker thread. Cheap
     * to call: it does nothing when there is nothing to load.
     */
    void requestFallbackFonts();
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &) const override;
    QModelIndex
    index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;

    void setVolumes(QList<FolderItem> *volumes);
    void setCurrentVolumeRow(int row);

private:
    void handleIconLoadFinished();
    void handleFallbackFontsLoaded();
    void applyIconImages(const FolderIconImages &images);
    void loadIconsFromProvider();
    void updatePlaceholderNames();

    QList<FolderItem> *m_searchedVolumes;
    int m_currentVolumeRow;
    QStringList m_placeholderNames;
    bool m_placeholdersActive = false;
    QSet<char32_t> m_glyphsToLoad;
    QFutureWatcher<void> m_fallbackFontWatcher;
    QFutureWatcher<FolderIconImages> m_iconWatcher;
    QIcon m_folderIcon;
    QIcon m_archiveIcon;
    QIcon m_imageIcon;
};

#endif // FOLDERITEMMODEL_H
