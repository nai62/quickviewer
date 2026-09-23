#include "volumenameparser.h"

TaggedName parseVolumeName(const QString &realname)
{
    // A volume name mixes the book title with the fields whoever made the name
    // put around it. The forms follow the comment this parser has carried
    // since the catalog was written:
    //
    //   (TAG1) [Publisher(Author)] book title (TAG2) (TAG3) ...
    //   # [TAG1] [TAG2] [Publisher(Author)] book title (TAG2) [TAG4] ...
    //
    // Both are meant to give the title "[Publisher(Author)] book title" and to
    // keep every other field as a tag:
    //
    //   - a parenthesized group is a tag of the volume,
    //   - a bracketed group holding "<publisher> (<author>)" is the publisher
    //     and the author: its text stays in the title, and it yields the
    //     publisher, the author and the pair as one tag each,
    //   - any other bracketed group is a tag of the volume,
    //   - the word right after a leading "#" is a tag of the volume.
    //
    // Tags are only what the name suggests: the catalog stores them when it
    // creates the volume, and the user can change them afterwards.
    TaggedName result;
    result.realname = realname;

    QString title;
    QString raw;         // text inside the group being read
    QChar group;         // nothing while outside a group, otherwise '(' or '['
    int depth = 0;       // nested parentheses inside a parenthesized group
    int authorFrom = -1; // where the parentheses start inside a bracketed group

    for (int index = 0; index < realname.size(); ++index) {
        const QChar c = realname.at(index);

        if (group == QLatin1Char('(')) {
            if (c == QLatin1Char('(')) {
                ++depth;
                raw += c;
            } else if (c == QLatin1Char(')') && depth > 0) {
                --depth;
                raw += c;
            } else if (c == QLatin1Char(')')) {
                const QString text = raw.trimmed();
                if (!text.isEmpty()) {
                    result.tags << TagRecord(text, 0); // Normal
                }
                raw.clear();
                group = QChar();
            } else {
                raw += c;
            }
            continue;
        }

        if (group == QLatin1Char('[')) {
            if (c == QLatin1Char(']')) {
                if (authorFrom >= 0) {
                    const QString publisher = raw.left(authorFrom).trimmed();
                    QString author = raw.mid(authorFrom + 1);
                    if (author.endsWith(QLatin1Char(')'))) {
                        author.chop(1);
                    }
                    author = author.trimmed();
                    if (!publisher.isEmpty()) {
                        result.tags << TagRecord(publisher, 2); // Publisher
                    }
                    if (!author.isEmpty()) {
                        result.tags << TagRecord(author, 3); // Author
                    }
                    const QString pair = raw.trimmed();
                    if (!pair.isEmpty()) {
                        result.tags << TagRecord(pair, 1); // Publisher(Author)
                    }
                    // The publisher and its author stay part of the title.
                    title += QLatin1Char('[') + raw + QLatin1Char(']');
                } else {
                    const QString text = raw.trimmed();
                    if (!text.isEmpty()) {
                        result.tags << TagRecord(text, 0); // Normal
                    }
                }
                raw.clear();
                authorFrom = -1;
                group = QChar();
            } else {
                if (c == QLatin1Char('(') && authorFrom < 0) {
                    authorFrom = raw.size();
                }
                raw += c;
            }
            continue;
        }

        if (c == QLatin1Char('[') || c == QLatin1Char('(')) {
            group = c;
            raw.clear();
            authorFrom = -1;
            depth = 0;
        } else if (c == QLatin1Char('#') && index == 0) {
            // The word after a leading "#" is a tag; "# [TAG]" only introduces
            // the tag, and the group that follows speaks for itself.
            int end = index + 1;
            while (end < realname.size() && !realname.at(end).isSpace()) {
                ++end;
            }
            if (end > index + 1 && realname.at(index + 1) != QLatin1Char('[')) {
                result.tags << TagRecord(realname.mid(index + 1, end - index - 1), 0); // Normal
                index = end - 1;
            }
        } else if (c.isSpace()) {
            if (!title.isEmpty() && !title.endsWith(QLatin1Char(' '))) {
                title += QLatin1Char(' ');
            }
        } else {
            title += c;
        }
    }

    // An unfinished field is literal title text, not a tag to discard.
    if (!group.isNull()) {
        title += group + raw;
    }
    result.name = title.trimmed();
    return result;
}
