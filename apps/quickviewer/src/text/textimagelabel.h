#ifndef TEXTIMAGELABEL_H
#define TEXTIMAGELABEL_H

#include <QtWidgets>
#include "textimagecache.h"

/**
 * A label for text that comes from the user's data, such as the folder panel's
 * caption or the page caption in the status bar.
 *
 * A path comes from the disk, so it can hold a character the UI font cannot
 * draw. Shaping such a path on the GUI thread loads a fallback font, which
 * stalls the window for a few hundred milliseconds the first time. This label
 * draws the text with the characters the font cannot draw stood in for, and
 * paints the image the text-image helper rasterized once it arrives. A
 * word-wrapped label has the helper wrap the text at the width it has; any
 * other label shows one line, elided as setElision() asks.
 *
 * Text set with QLabel::setText() is shown as it is: the label paints an image
 * only for the text it was given through setSourceText().
 */
class TextImageLabel : public QLabel
{
    Q_OBJECT
public:
    explicit TextImageLabel(QWidget *parent = nullptr);

    /** Shows \a text, standing in for the glyphs the UI font cannot draw. */
    void setSourceText(const QString &text);
    QString sourceText() const { return m_text; }
    /**
     * Elides a one-line label's text to \a width with \a mode. A width of zero
     * shows the whole line. Takes effect with the next setSourceText().
     */
    void setElision(Qt::TextElideMode mode, int width);
    /** True once the label paints the helper's image instead of its placeholder. */
    bool paintsTextImage() const;

    QSize sizeHint() const override;
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updatePlaceholder();
    void showText(const QString &text);
    void askForTextImage();
    void handleTextReady(const QByteArray &key, const TextImageResult &result);
    void applyTextImage(const QByteArray &key, const TextImageResult &result);
    TextImageLayout textLayout() const;
    QSize imageContentSize() const;

    QString m_text;
    /** \a m_text with a placeholder for the glyphs the font cannot draw, or empty. */
    QString m_safeText;
    /** What the label last put into QLabel::text(); anything else came from outside. */
    QString m_shownText;
    /** What the label paints: the helper's image, empty until it has answered. */
    QImage m_mask;
    QByteArray m_key;
    /** The layout \a m_key was made for, which the label has to ask for again. */
    int m_keyWidth = -1;
    Qt::TextElideMode m_elideMode = Qt::ElideNone;
    int m_elideWidth = 0;
};

#endif // TEXTIMAGELABEL_H
