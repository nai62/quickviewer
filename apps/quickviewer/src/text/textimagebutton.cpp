#include "textimagebutton.h"
#include "fallbacktext.h"

namespace {
/** The room the helper leaves around the text it rasterizes, on each side. */
constexpr int ImageMargin = 2;
} // namespace

TextImageButton::TextImageButton(QWidget *parent)
    : QPushButton(parent)
{
    connect(TextImageCache::instance(),
            &TextImageCache::finished,
            this,
            &TextImageButton::handleTextReady);
}

void TextImageButton::setSourceText(const QString &text)
{
    if (m_text == text && !text.isEmpty()) {
        return;
    }
    m_text = text;
    updatePlaceholder();
}

/**
 * Shows the caption, or the caption with a placeholder for every character the
 * button's font cannot draw. The font can still change when a style sheet
 * polishes the button, which shows it again.
 */
void TextImageButton::updatePlaceholder()
{
    m_safeText = FallbackText::placeholderFor(m_text, font());
    m_key.clear();
    m_mask = QImage();
    setText(m_safeText.isEmpty() ? m_text : m_safeText);
    // The button may stand in for characters on screen, but the name it
    // answers to is the caption itself.
    setAccessibleName(m_safeText.isEmpty() ? QString() : m_text);
    askForTextImage();
}

void TextImageButton::askForTextImage()
{
    if (m_safeText.isEmpty()) {
        return;
    }
    m_key = TextImageCache::key(m_text, font(), devicePixelRatioF());
    const TextImageResult cached = TextImageCache::instance()->lookup(m_key);
    if (cached) {
        handleTextReady(m_key, cached);
        return;
    }
    TextImageCache::instance()->request(m_key);
}

void TextImageButton::handleTextReady(const QByteArray &key, const TextImageResult &result)
{
    if (m_key.isEmpty()) {
        return;
    }
    if (key == m_key && result && result->images.size() == TextImages::ImageCount) {
        m_mask = result->images.at(font().bold() ? 1 : 0);
        updateGeometry();
        update();
        return;
    }
    if (m_mask.isNull() && !TextImageCache::instance()->pending(m_key)) {
        // The request may have been turned away while the cache held every
        // request it accepts at once; a place is free again.
        askForTextImage();
    }
}

QSize TextImageButton::imageContentSize() const
{
    const QSizeF size = m_mask.deviceIndependentSize();
    return QSize(qMax(0, qCeil(size.width()) - 2 * ImageMargin),
                 qMax(0, qCeil(size.height()) - 2 * ImageMargin));
}

QSize TextImageButton::sizeHint() const
{
    QSize size = QPushButton::sizeHint();
    if (paintsTextImage()) {
        // The button is laid out for its placeholder; the caption itself may
        // be wider.
        const int extra = imageContentSize().width() - fontMetrics().horizontalAdvance(text());
        size.rwidth() += qMax(0, extra);
    }
    return size;
}

void TextImageButton::paintEvent(QPaintEvent *event)
{
    if (!paintsTextImage()) {
        QPushButton::paintEvent(event);
        return;
    }
    // The button draws itself without a caption, and the helper's image of the
    // caption sits where the style puts the text.
    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    option.text.clear();
    painter.drawControl(QStyle::CE_PushButton, option);
    initStyleOption(&option);
    const QRect contents = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this);
    const QRect target =
        QStyle::alignedRect(layoutDirection(), Qt::AlignCenter, imageContentSize(), contents);
    const QImage image = tintedTextMask(m_mask, palette().color(QPalette::ButtonText));
    painter.drawImage(QPointF(target.topLeft()) - QPointF(ImageMargin, ImageMargin), image);
}

void TextImageButton::changeEvent(QEvent *event)
{
    QPushButton::changeEvent(event);
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::ApplicationFontChange:
    case QEvent::StyleChange:
    case QEvent::DevicePixelRatioChange:
        if (!m_text.isEmpty()) {
            // A style sheet sets the tag bar's font when it polishes a button.
            updatePlaceholder();
        }
        break;
    case QEvent::PaletteChange:
        update();
        break;
    default:
        break;
    }
}
