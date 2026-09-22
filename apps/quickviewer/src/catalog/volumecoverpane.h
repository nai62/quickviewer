#ifndef VOLUMECOVERPANE_H
#define VOLUMECOVERPANE_H

#include <QByteArray>
#include <QImage>
#include <QLabel>

/**
 * The cover of the book a catalog view has selected.
 *
 * The catalog stores a cover as a small JPEG, so the pane takes the bytes it
 * is given and fits the picture to the room it has whenever that room changes.
 * A book the catalog stored without a cover says so, and a pane with no book
 * to show is empty.
 */
class VolumeCoverPane : public QLabel
{
    Q_OBJECT
public:
    explicit VolumeCoverPane(QWidget *parent = nullptr);

    /** Shows the cover stored as \a stored, or that the book has none. */
    void setStoredCover(const QByteArray &stored);
    /** Shows nothing: there is no book to show a cover of. */
    void clearCover();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void apply();

    QImage m_cover;
    bool m_bookSelected = false;
};

#endif // VOLUMECOVERPANE_H
