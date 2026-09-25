#ifndef FOLDERPLACEHOLDERTEXT_H
#define FOLDERPLACEHOLDERTEXT_H

#include <QChar>
#include <QFont>
#include <QRawFont>
#include <QString>

/**
 * Display text for a name or a path whose characters the UI font may not cover.
 *
 * Qt loads the font it picks for a glyph the primary font does not have while
 * it shapes the text, and that load costs the GUI thread hundreds of
 * milliseconds the first time. The folder panel therefore checks the primary
 * font without merging, stands in for the characters it cannot draw, and lets
 * FolderTextCache rasterize the real text in another process.
 */
namespace FolderPlaceholderText {

/**
 * \a font as the panel checks it: the style strategy forbids merging, so asking
 * about a character never loads a fallback font. \a bold picks the weight.
 */
QRawFont primaryFont(const QFont &font, bool bold);

/**
 * True when both weights the panel draws have a glyph for \a code. A row or a
 * caption is drawn in either weight, so checking both keeps a placeholder that
 * would need a fallback font of its own out of the first paint.
 */
bool supportsGlyph(const QRawFont &normal, const QRawFont &bold, char32_t code);

/**
 * The character that stands in for a glyph the fonts cannot draw. '□' is what
 * the platform itself draws for a missing glyph, and the middle dot and '?'
 * behind it cover the fonts that lack the box.
 */
QChar placeholderCharacter(const QRawFont &normal, const QRawFont &bold);

/**
 * \a text with every character the fonts cannot draw replaced by
 * \a replacement, or an empty string when the text can be drawn as it is. One
 * placeholder stands in for one code point, so a surrogate pair gets a single
 * one instead of two.
 */
QString
placeholder(const QString &text, const QRawFont &normal, const QRawFont &bold, QChar replacement);

} // namespace FolderPlaceholderText

#endif // FOLDERPLACEHOLDERTEXT_H
