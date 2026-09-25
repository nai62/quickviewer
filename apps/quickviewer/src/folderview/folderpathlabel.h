#ifndef FOLDERPATHLABEL_H
#define FOLDERPATHLABEL_H

#include <QtWidgets>
#include "foldertextcache.h"

/**
 * The folder panel's caption: the path of the folder the panel shows.
 *
 * A path comes from the disk, so it can hold a character the UI font cannot
 * draw. Shaping such a path on the GUI thread loads a fallback font inside the
 * frame that reveals the window, which is the load the folder list keeps out of
 * its own paint. This label answers the same way: it draws the path with the
 * characters the font cannot draw stood in for, and paints the image the
 * folder-text helper rasterized once it arrives. The helper wraps that image at
 * the width the label has, so the caption reads as the path it names.
 */
class FolderPathLabel : public QLabel
{
    Q_OBJECT
public:
    explicit FolderPathLabel(QWidget *parent = nullptr);

    /** Shows \a path, standing in for the glyphs the UI font cannot draw. */
    void setPath(const QString &path);
    QString path() const { return m_path; }
    /** True once the caption paints the helper's image instead of its placeholder. */
    bool paintsPathImage() const { return !m_mask.isNull(); }

    QSize sizeHint() const override;
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updatePlaceholder();
    void askForTextImage();
    void handleTextReady(const QByteArray &key, const FolderTextResult &result);
    void applyTextImage(const QByteArray &key, const FolderTextResult &result);
    int wrapWidth() const;

    QString m_path;
    /** \a m_path with a placeholder for the glyphs the font cannot draw, or empty. */
    QString m_safeText;
    /** What the caption paints: the helper's image, empty until it has answered. */
    QImage m_mask;
    QByteArray m_key;
    /** The width \a m_key was wrapped at, which the label has to ask for again. */
    int m_keyWrapWidth = 0;
};

#endif // FOLDERPATHLABEL_H
