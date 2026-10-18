#include "textimagelabel.h"
#include "fallbacktext.h"

namespace {
/** The step a wrapped label asks the helper to wrap at: not every pixel of a drag. */
constexpr int WrapStep = 16;
/** The room the helper leaves around the text it rasterizes, on each side. */
constexpr int ImageMargin = 2;
/** The widest line the helper elides to. */
constexpr int MaxElideWidth = 4096;
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
    if (m_text == text && QLabel::text() == m_shownText) {
        return;
    }
    m_text = text;
    updatePlaceholder();
}

void TextImageLabel::setElision(Qt::TextElideMode mode, int width)
{
    width = qMax(0, width);
    if (mode == m_elideMode && width == m_elideWidth) {
        return;
    }
    m_elideMode = mode;
    m_elideWidth = width;
    if (!m_text.isEmpty() && QLabel::text() == m_shownText) {
        updatePlaceholder();
    }
}

bool TextImageLabel::paintsTextImage() const
{
    return !m_mask.isNull() && QLabel::text() == m_shownText;
}

/**
 * Shows the text, or the text with a placeholder for every character the
 * label's font cannot draw: shaping such a character is what loads a fallback
 * font, and the frame that reveals the window can paint this label.
 */
void TextImageLabel::updatePlaceholder()
{
    const QRawFont normal = FallbackText::primaryFont(font(), false);
    const QRawFont bold = FallbackText::primaryFont(font(), true);
    m_safeText = FallbackText::placeholder(
        m_text, normal, bold, FallbackText::placeholderCharacter(normal, bold));
    m_key.clear();
    m_keyWidth = -1;
    m_mask = QImage();
    showText(m_safeText.isEmpty() ? m_text : m_safeText);
    // The label may stand in for characters on screen, but the name it
    // answers to is the text itself. Text it shows whole answers for itself.
    setAccessibleName(m_safeText.isEmpty() ? QString() : m_text);
    updateGeometry();
    askForTextImage();
}

/** Puts \a text into the label, elided the way a one-line label is asked to. */
void TextImageLabel::showText(const QString &text)
{
    // Every character of the text is one the font draws itself - the real text
    // or its placeholder - so measuring it loads no fallback font.
    m_shownText = !wordWrap() && m_elideWidth > 0
                      ? fontMetrics().elidedText(text, m_elideMode, m_elideWidth)
                      : text;
    QLabel::setText(m_shownText);
}

/**
 * Asks the helper for the text laid out as the label shows it, and paints what
 * comes back. Asking is idempotent - a key in flight, a key with a result and a
 * key out of attempts are all left alone - so the label can ask again whenever
 * its width changed or a finished request may have freed a place in the queue
 * the cache accepts at once.
 */
void TextImageLabel::askForTextImage()
{
    if (m_safeText.isEmpty() || width() <= 0 || QLabel::text() != m_shownText) {
        return;
    }
    const TextImageLayout layout = textLayout();
    const QByteArray key = TextImageCache::key(m_text, font(), devicePixelRatioF(), layout);
    if (key != m_key) {
        // Another width wraps the text into other lines, so what the helper
        // rendered no longer sits where the label draws it: the placeholder
        // stands in until the text is rendered at this width.
        m_key = key;
        m_keyWidth = layout.width;
        if (!m_mask.isNull()) {
            m_mask = QImage();
            updateGeometry();
            update();
        }
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
 * How the helper lays the text out: wrapped at the width the label shows it
 * in, aligned to the side the label aligns it to, or one line elided the way
 * the label elides it.
 */
TextImageLayout TextImageLabel::textLayout() const
{
    TextImageLayout layout;
    if (wordWrap()) {
        const int room = width() - 2 * ImageMargin;
        layout.width = qMax(WrapStep, room / WrapStep * WrapStep);
        layout.flags =
            (QStyle::visualAlignment(layoutDirection(), alignment()) & Qt::AlignHorizontal_Mask) |
            Qt::TextWordWrap;
    } else if (m_elideWidth > 0) {
        layout.width = qMin(m_elideWidth, MaxElideWidth);
        layout.elideMode = m_elideMode;
    }
    return layout;
}

/** The size of the text in the helper's image, without the room around it. */
QSize TextImageLabel::imageContentSize() const
{
    const QSizeF size = m_mask.deviceIndependentSize();
    return QSize(qMax(0, qCeil(size.width()) - 2 * ImageMargin),
                 qMax(0, qCeil(size.height()) - 2 * ImageMargin));
}

void TextImageLabel::paintEvent(QPaintEvent *event)
{
    if (!paintsTextImage()) {
        QLabel::paintEvent(event);
        return;
    }
    // The frame still comes from the label; the text itself is the helper's,
    // so the GUI thread never shapes text it would load a fallback font for.
    QPainter painter(this);
    drawFrame(&painter);
    const QRect contents = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
    const QRect target =
        QStyle::alignedRect(layoutDirection(), alignment(), imageContentSize(), contents);
    const QImage image = tintedTextMask(m_mask, palette().color(foregroundRole()));
    painter.drawImage(QPointF(target.topLeft()) - QPointF(ImageMargin, ImageMargin), image);
}

void TextImageLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    if (m_safeText.isEmpty() || !wordWrap() || textLayout().width == m_keyWidth) {
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
        if (!m_text.isEmpty() && QLabel::text() == m_shownText) {
            // The font decides which characters need the helper, and the scale
            // decides what it rasterizes them at.
            updatePlaceholder();
        }
        break;
    case QEvent::LayoutDirectionChange:
        // Wrapped lines align to the side the label reads from.
        askForTextImage();
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
    if (paintsTextImage()) {
        // The image is the text, so it holds the room the label needs.
        const QMargins margins =
            contentsMargins() + QMargins(margin(), margin(), margin(), margin());
        return imageContentSize().grownBy(margins);
    }
    return QLabel::sizeHint();
}

int TextImageLabel::heightForWidth(int width) const
{
    if (paintsTextImage() && wordWrap()) {
        const QMargins margins = contentsMargins();
        return imageContentSize().height() + margins.top() + margins.bottom() + 2 * margin();
    }
    return QLabel::heightForWidth(width);
}
