#include "fallbacktext.h"

#include <QHash>

namespace FallbackText {
QRawFont primaryFont(const QFont &font, bool bold)
{
    QFont primary = font;
    if (bold) {
        primary.setBold(true);
    }
    primary.setStyleStrategy(QFont::StyleStrategy(primary.styleStrategy() | QFont::NoFontMerging));
    return QRawFont::fromFont(primary);
}

bool supportsGlyph(const QRawFont &normal, const QRawFont &bold, char32_t code)
{
    return normal.isValid() && bold.isValid() && normal.supportsCharacter(code) &&
           bold.supportsCharacter(code);
}

QChar placeholderCharacter(const QRawFont &normal, const QRawFont &bold)
{
    for (char32_t candidate : {char32_t(0x25a1), char32_t(0x00b7), U'?'}) {
        if (supportsGlyph(normal, bold, candidate)) {
            return QChar(candidate);
        }
    }
    return QLatin1Char('?');
}

QString
placeholder(const QString &text, const QRawFont &normal, const QRawFont &bold, QChar replacement)
{
    bool asciiOnly = true;
    for (const QChar &character : text) {
        if (character.unicode() >= 0x80) {
            asciiOnly = false;
            break;
        }
    }
    if (asciiOnly) {
        return QString();
    }
    QString result;
    bool replaced = false;
    for (char32_t code : text.toUcs4()) {
        if (code < 0x80 || supportsGlyph(normal, bold, code)) {
            result += QString::fromUcs4(&code, 1);
        } else {
            result += replacement;
            replaced = true;
        }
    }
    return replaced ? result : QString();
}

QString placeholderFor(const QString &text, const QFont &font)
{
    struct Fonts
    {
        QRawFont normal;
        QRawFont bold;
        QChar replacement;
    };
    static QHash<QString, Fonts> fontsByKey;
    // A wide text is checked character by character against the fonts, so a
    // plain one is let through before any font is looked up.
    bool asciiOnly = true;
    for (const QChar &character : text) {
        if (character.unicode() >= 0x80) {
            asciiOnly = false;
            break;
        }
    }
    if (asciiOnly) {
        return QString();
    }
    const QString key = font.key();
    auto found = fontsByKey.constFind(key);
    if (found == fontsByKey.cend()) {
        if (fontsByKey.size() >= 32) {
            fontsByKey.clear();
        }
        Fonts fonts{primaryFont(font, false), primaryFont(font, true), QChar()};
        fonts.replacement = placeholderCharacter(fonts.normal, fonts.bold);
        found = fontsByKey.insert(key, fonts);
    }
    return placeholder(text, found->normal, found->bold, found->replacement);
}

} // namespace FallbackText
