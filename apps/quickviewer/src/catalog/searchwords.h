#ifndef SEARCHWORDS_H
#define SEARCHWORDS_H

#include <QString>
#include <QStringList>

/**
 * One search of the catalog list, as the user typed it.
 *
 * A search is the words that all have to appear in a title, and the words that
 * must not appear at all: a word written with a leading '-' only excludes. A
 * lone '-' stays an ordinary word, so a title that holds one can be searched
 * for.
 *
 * Case is folded here and in the title the search is matched against, so a
 * caller that keeps the folded title of a book can hand it over as it is.
 */
class SearchWords
{
public:
    explicit SearchWords(const QString &search);

    /** True while the search asks for every title. */
    bool matchesEverything() const { return m_matches.isEmpty() && m_noMatches.isEmpty(); }

    /** Whether the folded title \a titleNoCase is one the user asked for. */
    bool match(const QString &titleNoCase) const;

private:
    QStringList m_matches;
    QStringList m_noMatches;
};

#endif // SEARCHWORDS_H
