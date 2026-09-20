#ifndef FOLDERITEMMODEL_H
#define FOLDERITEMMODEL_H

#include <QtWidgets>
#include <QtCore>

#include "folderitem.h"
#include "foldertextcache.h"

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
    enum ItemRole { CurrentVolumeRole = Qt::UserRole, SafeTextRole, TextImagesRole };

    FolderItemModel(QObject *parent, FolderTextCache *textCache = nullptr);
    /**
     * Starts loading the list icons in the background. Called while the window
     * is still being created, so the panel does not have to wait for the shell
     * when it appears.
     */
    static void startIconLoad();
    void setTextStyle(const QFont &font, const QPalette &palette, qreal devicePixelRatio);
    void requestTextImages();
    bool textImagesPending() const;
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
    FolderTextCache *m_textCache;
    QFont m_textFont;
    QPalette m_textPalette;
    qreal m_textRatio = 1;
    QList<QByteArray> m_textKeys;
    QList<FolderTextResult> m_textImages;
    QFutureWatcher<FolderIconImages> m_iconWatcher;
    QIcon m_folderIcon;
    QIcon m_archiveIcon;
    QIcon m_imageIcon;
};

#endif // FOLDERITEMMODEL_H
