#ifndef TEXTIMAGELABEL_H
#define TEXTIMAGELABEL_H

#include <QtWidgets>
#include "textimagecache.h"

/**
 * A label for text that comes from the user's data, such as the folder panel's
 * caption: the path of the folder the panel shows.
 *
 * A path comes from the disk, so it can hold a character the UI font cannot
 * draw. Shaping such a path on the GUI thread loads a fallback font inside the
 * frame that reveals the window, which is the load the folder list keeps out of
 * its own paint. This label answers the same way: it draws the text with the
 * characters the font cannot draw stood in for, and paints the image the
 * text-image helper rasterized once it arrives. The helper wraps that image at
 * the width the label has, so the label reads as the text it names.
 */
class TextImageLabel : public QLabel
{
    Q_OBJECT
public:
    explicit TextImageLabel(QWidget *parent = nullptr);

    /** Shows \a text, standing in for the glyphs the UI font cannot draw. */
    void setSourceText(const QString &text);
    QString sourceText() const { return m_text; }
    /** True once the label paints the helper's image instead of its placeholder. */
    bool paintsTextImage() const { return !m_mask.isNull(); }

    QSize sizeHint() const override;
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updatePlaceholder();
    void askForTextImage();
    void handleTextReady(const QByteArray &key, const TextImageResult &result);
    void applyTextImage(const QByteArray &key, const TextImageResult &result);
    int wrapWidth() const;

    QString m_text;
    /** \a m_text with a placeholder for the glyphs the font cannot draw, or empty. */
    QString m_safeText;
    /** What the caption paints: the helper's image, empty until it has answered. */
    QImage m_mask;
    QByteArray m_key;
    /** The width \a m_key was wrapped at, which the label has to ask for again. */
    int m_keyWrapWidth = 0;
};

#endif // TEXTIMAGELABEL_H
