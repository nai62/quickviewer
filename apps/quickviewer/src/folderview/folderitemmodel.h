#ifndef FOLDERITEMMODEL_H
#define FOLDERITEMMODEL_H

#include <QRawFont>
#include <QtWidgets>
#include <QtCore>

#include "folderitem.h"
#include "foldertextcache.h"
#include "models/readprogressstore.h"

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
    enum ItemRole {
        CurrentVolumeRole = Qt::UserRole,
        SafeTextRole,
        TextImagesRole,
        ReadProgressRole
    };

    FolderItemModel(QObject *parent, FolderTextCache *textCache = nullptr);
    /**
     * Starts loading the list icons in the background. Called while the window
     * is still being created, so the panel does not have to wait for the shell
     * when it appears.
     */
    static void startIconLoad();
    void setTextStyle(const QFont &font, const QPalette &palette, qreal devicePixelRatio);
    /**
     * Rows the list currently shows, plus its margin. Only these ask for text
     * images, so a folder with thousands of names renders what is on screen.
     * A negative last row means the list has not reported a range yet.
     */
    void setVisibleRowRange(int first, int last);
    /** Asks for the text of the rows in the visible range. */
    void requestTextImages();
    /** Asks for every row; the folder-text profile settles the whole folder. */
    void requestAllTextImages();
    bool textImagesPending() const;
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &) const override;
    QModelIndex
    index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;

    void setVolumes(QList<FolderItem> *volumes);
    void setCurrentVolumeRow(int row);
    /**
     * Replaces every row's read progress; rows that are not listed have none.
     * The store reads its settings in the background, so a listed panel is told
     * again once they arrive.
     */
    void setReadProgress(const QHash<int, ReadProgress> &progressByRow);
    /** Replaces one row's read progress and repaints that row. */
    void updateReadProgress(int row, const ReadProgress &progress);

private:
    void handleIconLoadFinished();
    void applyIconImages(const FolderIconImages &images);
    void loadIconsFromProvider();
    void requestTextImagesInRange(int first, int last);
    void updatePrimaryFontGlyphs();
    void updatePlaceholderNames();

    QList<FolderItem> *m_searchedVolumes;
    int m_currentVolumeRow;
    QStringList m_placeholderNames;
    FolderTextCache *m_textCache;
    QFont m_textFont;
    QPalette m_textPalette;
    qreal m_textRatio = 1;
    QRawFont m_primaryNormal;
    QRawFont m_primaryBold;
    QChar m_replacement = QLatin1Char('?');
    int m_firstVisibleRow = 0;
    int m_lastVisibleRow = -1;
    QList<QByteArray> m_textKeys;
    QList<FolderTextResult> m_textImages;
    QHash<int, ReadProgress> m_readProgress;
    QFutureWatcher<FolderIconImages> m_iconWatcher;
    QIcon m_folderIcon;
    QIcon m_archiveIcon;
    QIcon m_imageIcon;
};

#endif // FOLDERITEMMODEL_H
