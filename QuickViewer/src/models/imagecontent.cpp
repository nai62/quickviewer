#include "imagecontent.h"

void ImageContent::initializeAnimation()
{
    if (!movie.isNull() && !movie.data()) {
        movie.load();
        QMovie *qtMovie = movie.data();
        qtMovie->jumpToFrame(0);
        const QPixmap firstFrame = qtMovie->currentPixmap();
        loadedImage = firstFrame.toImage();
        originalSize = loadedImageSize = firstFrame.size();
    }
}
