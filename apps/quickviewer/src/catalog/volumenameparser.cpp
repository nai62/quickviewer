#include "volumenameparser.h"

namespace {

/**
 * The text of a tag, without the brackets a volume name may wrap it in. The
 * parser buffers the text it is reading, and only some of the forms it accepts
 * include their delimiters, so both are trimmed only where they are there.
 */
QString unwrappedTag(QString text)
{
    if (text.startsWith(QLatin1Char('['))) {
        text.remove(0, 1);
    }
    if (text.endsWith(QLatin1Char(']'))) {
        text.chop(1);
    }
    return text;
}

} // namespace

TaggedName parseVolumeName(const QString &realname)
{
    // Extract book title from folder name
    // from: <<<(TAG1) [Publisher(Author)] book title (TAG2) (TAG3) ...>>>
    //   to: <<<[Publisher(Author)] book title>>>
    //
    // e.g. 'Star Wars - Han Solo (2017) (Digital) (newcomic.info)'
    //
    // from: <<<# [TAG1] [TAG2] [Publisher(Author)] book title (TAG2) [TAG4] ...>>>
    //   to: <<<[Publisher(Author)] book title>>>
    //
    // TAGs will save other fields

    TaggedName result;
    result.realname = realname;
    QList<QChar> parenthesis;
    parenthesis << '?';
    QStringList clist;
    QStringList tag;
    int cnt = 0;
    bool NumberSign = false;
    bool authorExported = false;
    int type_id = 0;
    for (QChar c : realname) {
        switch (c.unicode()) {
        case '#':
            if (cnt == 0) {
                NumberSign = true;
                parenthesis << c;
            } else if (parenthesis.last() == '#') {
                tag << c;
            } else {
                clist << c;
            }
            break;
        case '[':
            parenthesis << c;
            if (tag.size()) {
                if (tag[0] == "[") {
                    QString publisher = tag.join("");
                    result.tags << TagRecord(unwrappedTag(publisher), type_id); // Normal
                } else {
                    result.tags << TagRecord(tag.join(""), type_id);
                }
                tag.clear();
            }
            type_id = NumberSign ? 0 : 2;
            tag << c;
            break;
        case ']':
            if (parenthesis.size() == 1) {
                break;
            }
            parenthesis.removeLast();
            tag << c;
            if (!NumberSign && !authorExported && tag.size()) {
                clist << tag.join("");
                QString pubauthor = tag.join("");
                result.tags << TagRecord(unwrappedTag(pubauthor), type_id); // Publisher(Author)
                type_id = 0;
                tag.clear();
                authorExported = true;
            }
            break;
        case '(':
            if (parenthesis.last() == '[' && tag.size() >= 2) {
                QString publisher = tag.join("");
                result.tags << TagRecord(unwrappedTag(publisher), 2); // Publisher
                type_id = 1;
                tag << c;
            } else {
                tag.clear();
                if (parenthesis.last() == '#') {
                    parenthesis.removeLast();
                    NumberSign = false;
                }
                parenthesis << c;
            }
            break;
        case ')':
            if (parenthesis.size() == 1) {
                break;
            }
            if (parenthesis.last() == '[') {
                QString author = tag.join("");
                result.tags << TagRecord(author.mid(author.indexOf('(') + 1), 3); // Author
                tag << c;
            } else {
                if (tag.size()) {
                    result.tags << TagRecord(tag.join(""), 0); // Normal
                    tag.clear();
                }
                parenthesis.removeLast();
            }
            break;
        default:
            if (parenthesis.last() == '[') {
                tag << c;
            } else {
                if (parenthesis.last() == '#') {
                    if (c != ' ') {
                        tag << c;
                    } else {
                        parenthesis.removeLast();
                    }
                } else if (NumberSign && c != ' ' && tag.size()) {
                    // last tag will be Publisher/Author
                    QString pubauthor = tag.join("");
                    result.tags << TagRecord(unwrappedTag(pubauthor),
                                             pubauthor.indexOf("(") > 0 ? 1
                                                                        : 2); // Publisher(Author)
                    clist << tag.join("") << " " << c;
                    tag.clear();
                    NumberSign = false;
                } else if (parenthesis.last() == '(') {
                    tag << c;
                } else {
                    clist << c;
                }
            }
        }
        cnt++;
    }
    result.name = clist.join("").trimmed();
    return result;
}
