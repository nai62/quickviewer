#include "imagecontent.h"

void ImageContent::initializeAnimation()
{
    if (movie.isNull() || movie.data()) {
        return;
    }
    movie.load();
    QMovie *qtMovie = movie.data();
    if (!qtMovie) {
        return;
    }
    // A movie that cannot show its first frame keeps the size the decoder
    // reported instead of losing it to an empty frame.
    if (qtMovie->jumpToFrame(0)) {
        const QPixmap firstFrame = qtMovie->currentPixmap();
        loadedImage = firstFrame.toImage();
        originalSize = loadedImageSize = firstFrame.size();
    }
}
