#include "folderpathlabel.h"
#include "folderplaceholdertext.h"

namespace {
/** The step the caption asks the helper to wrap at: not every pixel of a drag. */
constexpr int WrapStep = 16;
/** The room the helper leaves around the text it rasterizes. */
constexpr int ImageMargin = 4;
} // namespace

FolderPathLabel::FolderPathLabel(QWidget *parent)
    : QLabel(parent)
{
    setTextFormat(Qt::PlainText);
    connect(FolderTextCache::instance(),
            &FolderTextCache::finished,
            this,
            &FolderPathLabel::handleTextReady);
}

void FolderPathLabel::setPath(const QString &path)
{
    if (m_path == path) {
        return;
    }
    m_path = path;
    updatePlaceholder();
}

/**
 * Shows the path, or the path with a placeholder for every character the
 * label's font cannot draw: shaping such a character is what loads a fallback
 * font, and the frame that reveals the window paints this label.
 */
void FolderPathLabel::updatePlaceholder()
{
    const QRawFont normal = FolderPlaceholderText::primaryFont(font(), false);
    const QRawFont bold = FolderPlaceholderText::primaryFont(font(), true);
    const QString replaced = FolderPlaceholderText::placeholder(
        m_path, normal, bold, FolderPlaceholderText::placeholderCharacter(normal, bold));
    m_safeText = replaced.isEmpty() ? QString() : replaced;
    m_key.clear();
    m_keyWrapWidth = 0;
    m_mask = QImage();
    setText(replaced.isEmpty() ? m_path : replaced);
    // The caption may stand in for characters on screen, but the name it
    // answers to is the path on disk.
    setAccessibleName(m_path);
    updateGeometry();
    askForTextImage();
}

/**
 * Asks the helper for the path rasterized at the width the caption has, and
 * paints what comes back. Asking is idempotent - a key in flight, a key with a
 * result and a key out of attempts are all left alone - so the caption can ask
 * again whenever its width changed or a finished request may have freed a place
 * in the queue the cache accepts at once.
 */
void FolderPathLabel::askForTextImage()
{
    if (m_safeText.isEmpty() || width() <= 0) {
        return;
    }
    const int wrap = wrapWidth();
    const QByteArray key = FolderTextCache::key(m_path, font(), devicePixelRatioF(), wrap);
    if (key != m_key) {
        // Another width wraps the path into other lines, so what the helper
        // rendered no longer sits where the label draws it: the placeholder
        // stands in until the path is rendered at this width.
        m_key = key;
        m_keyWrapWidth = wrap;
        m_mask = QImage();
        setText(m_safeText);
        updateGeometry();
        update();
    }
    const FolderTextResult cached = FolderTextCache::instance()->lookup(m_key);
    if (cached) {
        applyTextImage(m_key, cached);
        return;
    }
    FolderTextCache::instance()->request(m_key);
}

void FolderPathLabel::handleTextReady(const QByteArray &key, const FolderTextResult &result)
{
    FolderTextCache *cache = FolderTextCache::instance();
    if (key == m_key) {
        applyTextImage(key, result);
    }
    if (m_mask.isNull() && !m_key.isEmpty() && !cache->pending(m_key)) {
        // The caption's own request may have been turned away while the cache
        // held every request it accepts at once; a place is free again.
        askForTextImage();
    }
}

void FolderPathLabel::applyTextImage(const QByteArray &key, const FolderTextResult &result)
{
    if (key != m_key) {
        return;
    }
    const int weight = font().bold() ? 1 : 0;
    if (!result || result->images.size() != FolderTextImages::ImageCount ||
        result->images.at(weight).isNull()) {
        // The helper did not answer: the placeholder stands in for the path.
        return;
    }
    m_mask = result->images.at(weight);
    updateGeometry();
    update();
}

/**
 * The width the caption shows the path in. The helper wraps the path at this
 * width, so the image covers the lines the label draws.
 */
int FolderPathLabel::wrapWidth() const
{
    const int room = width() - ImageMargin;
    return qMax(WrapStep, room / WrapStep * WrapStep);
}

void FolderPathLabel::paintEvent(QPaintEvent *event)
{
    if (m_mask.isNull()) {
        QLabel::paintEvent(event);
        return;
    }
    // The frame still comes from the label; the path itself is the helper's, so
    // the GUI thread never shapes a path it would load a fallback font for.
    QPainter painter(this);
    drawFrame(&painter);
    const QImage image = tintedTextMask(m_mask, palette().color(QPalette::WindowText));
    const QSizeF size = image.deviceIndependentSize();
    painter.drawImage(QPointF(0, qMax(0, int((height() - size.height()) / 2))), image);
}

void FolderPathLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    if (m_safeText.isEmpty() || wrapWidth() == m_keyWrapWidth) {
        return;
    }
    askForTextImage();
}

void FolderPathLabel::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::ApplicationFontChange:
    case QEvent::StyleChange:
    case QEvent::DevicePixelRatioChange:
        if (!m_path.isEmpty()) {
            // The font decides which characters need the helper, and the scale
            // decides what it rasterizes them at.
            updatePlaceholder();
        }
        break;
    case QEvent::PaletteChange:
        // The image is tinted with the colour the caption draws in.
        update();
        break;
    default:
        break;
    }
}

QSize FolderPathLabel::sizeHint() const
{
    if (!m_mask.isNull()) {
        // The image is the path, so it holds the room the caption needs.
        const QSizeF size = m_mask.deviceIndependentSize();
        return QSize(qCeil(size.width()), qCeil(size.height()));
    }
    return QLabel::sizeHint();
}

int FolderPathLabel::heightForWidth(int width) const
{
    if (!m_mask.isNull()) {
        return qCeil(m_mask.deviceIndependentSize().height());
    }
    return QLabel::heightForWidth(width);
}
