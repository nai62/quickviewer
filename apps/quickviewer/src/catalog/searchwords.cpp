#include "searchwords.h"

SearchWords::SearchWords(const QString &search)
{
    for (const QString &word : search.toLower().trimmed().split(QLatin1Char(' '))) {
        if (word.isEmpty()) {
            continue;
        }
        if (word.size() > 1 && word.at(0) == QLatin1Char('-')) {
            m_noMatches << word.mid(1);
        } else {
            m_matches << word;
        }
    }
}

bool SearchWords::match(const QString &titleNoCase) const
{
    for (const QString &word : m_matches) {
        if (!titleNoCase.contains(word)) {
            return false;
        }
    }
    for (const QString &word : m_noMatches) {
        if (titleNoCase.contains(word)) {
            return false;
        }
    }
    return true;
}
