#include "textimagelabel.h"
#include "fallbacktext.h"

namespace {
/** The step the label asks the helper to wrap at: not every pixel of a drag. */
constexpr int WrapStep = 16;
/** The room the helper leaves around the text it rasterizes. */
constexpr int ImageMargin = 4;
} // namespace

TextImageLabel::TextImageLabel(QWidget *parent)
    : QLabel(parent)
{
    setTextFormat(Qt::PlainText);
    connect(TextImageCache::instance(),
            &TextImageCache::finished,
            this,
            &TextImageLabel::handleTextReady);
}

void TextImageLabel::setSourceText(const QString &text)
{
    if (m_text == text) {
        return;
    }
    m_text = text;
    updatePlaceholder();
}

/**
 * Shows the text, or the text with a placeholder for every character the
 * label's font cannot draw: shaping such a character is what loads a fallback
 * font, and the frame that reveals the window paints this label.
 */
void TextImageLabel::updatePlaceholder()
{
    const QRawFont normal = FallbackText::primaryFont(font(), false);
    const QRawFont bold = FallbackText::primaryFont(font(), true);
    const QString replaced = FallbackText::placeholder(
        m_text, normal, bold, FallbackText::placeholderCharacter(normal, bold));
    m_safeText = replaced.isEmpty() ? QString() : replaced;
    m_key.clear();
    m_keyWrapWidth = 0;
    m_mask = QImage();
    setText(replaced.isEmpty() ? m_text : replaced);
    // The label may stand in for characters on screen, but the name it
    // answers to is the text itself.
    setAccessibleName(m_text);
    updateGeometry();
    askForTextImage();
}

/**
 * Asks the helper for the text rasterized at the width the label has, and
 * paints what comes back. Asking is idempotent - a key in flight, a key with a
 * result and a key out of attempts are all left alone - so the label can ask
 * again whenever its width changed or a finished request may have freed a place
 * in the queue the cache accepts at once.
 */
void TextImageLabel::askForTextImage()
{
    if (m_safeText.isEmpty() || width() <= 0) {
        return;
    }
    const int wrap = wrapWidth();
    const QByteArray key = TextImageCache::key(m_text, font(), devicePixelRatioF(), wrap);
    if (key != m_key) {
        // Another width wraps the text into other lines, so what the helper
        // rendered no longer sits where the label draws it: the placeholder
        // stands in until the text is rendered at this width.
        m_key = key;
        m_keyWrapWidth = wrap;
        m_mask = QImage();
        setText(m_safeText);
        updateGeometry();
        update();
    }
    const TextImageResult cached = TextImageCache::instance()->lookup(m_key);
    if (cached) {
        applyTextImage(m_key, cached);
        return;
    }
    TextImageCache::instance()->request(m_key);
}

void TextImageLabel::handleTextReady(const QByteArray &key, const TextImageResult &result)
{
    TextImageCache *cache = TextImageCache::instance();
    if (key == m_key) {
        applyTextImage(key, result);
    }
    if (m_mask.isNull() && !m_key.isEmpty() && !cache->pending(m_key)) {
        // The label's own request may have been turned away while the cache
        // held every request it accepts at once; a place is free again.
        askForTextImage();
    }
}

void TextImageLabel::applyTextImage(const QByteArray &key, const TextImageResult &result)
{
    if (key != m_key) {
        return;
    }
    const int weight = font().bold() ? 1 : 0;
    if (!result || result->images.size() != TextImages::ImageCount ||
        result->images.at(weight).isNull()) {
        // The helper did not answer: the placeholder stands in for the text.
        return;
    }
    m_mask = result->images.at(weight);
    updateGeometry();
    update();
}

/**
 * The width the label shows the text in. The helper wraps the text at this
 * width, so the image covers the lines the label draws.
 */
int TextImageLabel::wrapWidth() const
{
    const int room = width() - ImageMargin;
    return qMax(WrapStep, room / WrapStep * WrapStep);
}

void TextImageLabel::paintEvent(QPaintEvent *event)
{
    if (m_mask.isNull()) {
        QLabel::paintEvent(event);
        return;
    }
    // The frame still comes from the label; the text itself is the helper's, so
    // the GUI thread never shapes text it would load a fallback font for.
    QPainter painter(this);
    drawFrame(&painter);
    const QImage image = tintedTextMask(m_mask, palette().color(QPalette::WindowText));
    const QSizeF size = image.deviceIndependentSize();
    painter.drawImage(QPointF(0, qMax(0, int((height() - size.height()) / 2))), image);
}

void TextImageLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    if (m_safeText.isEmpty() || wrapWidth() == m_keyWrapWidth) {
        return;
    }
    askForTextImage();
}

void TextImageLabel::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::ApplicationFontChange:
    case QEvent::StyleChange:
    case QEvent::DevicePixelRatioChange:
        if (!m_text.isEmpty()) {
            // The font decides which characters need the helper, and the scale
            // decides what it rasterizes them at.
            updatePlaceholder();
        }
        break;
    case QEvent::PaletteChange:
        // The image is tinted with the colour the label draws in.
        update();
        break;
    default:
        break;
    }
}

QSize TextImageLabel::sizeHint() const
{
    if (!m_mask.isNull()) {
        // The image is the text, so it holds the room the label needs.
        const QSizeF size = m_mask.deviceIndependentSize();
        return QSize(qCeil(size.width()), qCeil(size.height()));
    }
    return QLabel::sizeHint();
}

int TextImageLabel::heightForWidth(int width) const
{
    if (!m_mask.isNull()) {
        return qCeil(m_mask.deviceIndependentSize().height());
    }
    return QLabel::heightForWidth(width);
}
