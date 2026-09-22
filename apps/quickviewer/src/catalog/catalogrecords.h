#ifndef CATALOGRECORDS_H
#define CATALOGRECORDS_H

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

// t_catalogs
class CatalogRecord
{
public:
    int id = 0;
    int basevolume_id = 0;
    QString name;
    QString path;
    QDateTime created_at;
    QDateTime updated_at;
    bool created = false;
    bool operator==(const CatalogRecord &rhs) { return id == rhs.id; }
};
Q_DECLARE_METATYPE(CatalogRecord)

// v_volumethm
class VolumeThumbRecord
{
public:
    // Every field starts at a value: a record is built by filling in the
    // columns a query returns, and a caller must not read what it did not fill.
    int id = 0;
    QString name;
    QString nameNoCase;
    QString realname;
    QString realnameNoCase;
    QString path;
    int thumb_id = 0;
    QByteArray thumbnail;
    QStringList tags;
};

// t_tags
class TagRecord
{
public:
    int id;
    QString name;
    QString nameNoCase;
    int type_id; // (0:Normal, 1:Publisher(Author), 2:Publisher, 3:Author, 4:Rate)
    TagRecord()
        : id(-1),
          type_id(0)
    {
    }
    TagRecord(QString nm, int tpid)
        : id(-1),
          name(nm),
          type_id(tpid)
    {
    }
    inline const TagRecord &operator=(const TagRecord &rhs)
    {
        id = rhs.id;
        name = rhs.name;
        nameNoCase = rhs.nameNoCase;
        type_id = rhs.type_id;
        return rhs;
    }
};

#endif // CATALOGRECORDS_H
