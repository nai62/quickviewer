#ifndef TEXTIMAGEBUTTON_H
#define TEXTIMAGEBUTTON_H

#include <QtWidgets>
#include "textimagecache.h"

/**
 * A push button whose caption comes from the user's data, such as a tag of the
 * catalog's tag bar.
 *
 * Like TextImageLabel, the button shows a caption the UI font cannot draw with
 * a placeholder, and paints the helper's image of it once that arrives, so the
 * GUI thread never loads a fallback font for it. text() holds what is shown;
 * sourceText() is the caption itself.
 */
class TextImageButton : public QPushButton
{
    Q_OBJECT
public:
    explicit TextImageButton(QWidget *parent = nullptr);

    /** Shows \a text, standing in for the glyphs the button's font cannot draw. */
    void setSourceText(const QString &text);
    QString sourceText() const { return m_text; }
    /** True once the button paints the helper's image instead of its placeholder. */
    bool paintsTextImage() const { return !m_mask.isNull(); }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updatePlaceholder();
    void askForTextImage();
    void handleTextReady(const QByteArray &key, const TextImageResult &result);
    QSize imageContentSize() const;

    QString m_text;
    /** \a m_text with a placeholder for the glyphs the font cannot draw, or empty. */
    QString m_safeText;
    /** What the button paints: the helper's image, empty until it has answered. */
    QImage m_mask;
    QByteArray m_key;
};

#endif // TEXTIMAGEBUTTON_H
