#ifndef VOLUMENAMEPARSER_H
#define VOLUMENAMEPARSER_H

#include <QList>
#include <QString>

#include "catalogrecords.h"

/**
 * A volume name split into the title a catalog shows and the tags the name
 * carries.
 */
class TaggedName
{
public:
    QString name;
    QString realname;
    QList<TagRecord> tags;
};

/**
 * Reads the title and the tags of the volume named \a realname.
 *
 * The tags are only what the name suggests: a catalog stores them when it
 * creates the volume, and the user can change them afterwards. See
 * developer/Catalogue.md for the rules this follows.
 */
TaggedName parseVolumeName(const QString &realname);

#endif // VOLUMENAMEPARSER_H
