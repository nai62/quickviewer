#include "volumecoverpane.h"

#include <QResizeEvent>

#include "fileloader.h"

VolumeCoverPane::VolumeCoverPane(QWidget *parent)
    : QLabel(parent)
{
}

void VolumeCoverPane::setStoredCover(const QByteArray &stored)
{
    m_cover = QImage::fromData(stored, IFileLoader::jpegQtFormatName());
    m_bookSelected = true;
    apply();
}

void VolumeCoverPane::clearCover()
{
    m_cover = QImage();
    m_bookSelected = false;
    apply();
}

void VolumeCoverPane::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    apply();
}

void VolumeCoverPane::apply()
{
    if (m_cover.isNull()) {
        setPixmap(QPixmap());
        setText(m_bookSelected ? tr("No cover", "Text shown where a cover would be") : QString());
        return;
    }
    setText(QString());
    // The page keeps its shape: the pane is often a different shape than the
    // cover it has to hold.
    setPixmap(
        QPixmap::fromImage(m_cover.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}
