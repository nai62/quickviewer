#ifndef CATALOGRECORDS_H
#define CATALOGRECORDS_H

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QString>

// t_catalogs
class CatalogRecord
{
public:
    int id;
    int basevolume_id;
    QString name;
    QString description;
    QString path;
    QDateTime created_at;
    QDateTime updated_at;
    bool created;
    bool operator==(const CatalogRecord &rhs) { return id == rhs.id; }
};
Q_DECLARE_METATYPE(CatalogRecord)

// v_volumethm
class VolumeThumbRecord
{
public:
    int id;
    QString name;
    QString nameNoCase;
    QString realname;
    QString realnameNoCase;
    QString path;
    int frontpage_id;
    int thumb_id;
    int parent_id;
    int catalog_id;
    QByteArray thumbnail;
};

// t_tags
class TagRecord
{
public:
    int id;
    QString name;
    QString nameNoCase;
    int type_id; // (0:Normal, 1:Publisher(Author), 2:Publisher, 3:Author, 4:Rate)
    int count;
    TagRecord()
        : id(-1),
          type_id(0),
          count(0)
    {
    }
    TagRecord(QString nm, int tpid)
        : id(-1),
          name(nm),
          type_id(tpid),
          count(0)
    {
    }
    inline const TagRecord &operator=(const TagRecord &rhs)
    {
        id = rhs.id;
        name = rhs.name;
        nameNoCase = rhs.nameNoCase;
        type_id = rhs.type_id;
        count = rhs.count;
        return rhs;
    }
};

#endif // CATALOGRECORDS_H
