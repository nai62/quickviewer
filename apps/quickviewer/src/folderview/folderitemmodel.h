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
     * Shows a placeholder for characters the UI font cannot draw yet. Qt loads a
     * fallback font for them on the GUI thread, which costs a few hundred
     * milliseconds, so a list holding such a name can be shown immediately and
     * completed once that font is available.
     */
    void setPlaceholderNames(bool enabled);
    /**
     * Names, in display order, that need a fallback font. The rows without one
     * are empty entries.
     */
    QStringList namesNeedingFallback() const;
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
    void applyIconImages(const FolderIconImages &images);
    void loadIconsFromProvider();
    void updatePlaceholderNames();

    QList<FolderItem> *m_searchedVolumes;
    int m_currentVolumeRow;
    QStringList m_placeholderNames;
    bool m_placeholdersActive = false;
    QFutureWatcher<FolderIconImages> m_iconWatcher;
    QIcon m_folderIcon;
    QIcon m_archiveIcon;
    QIcon m_imageIcon;
};

#endif // FOLDERITEMMODEL_H
