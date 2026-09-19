#ifndef MOVIE_H
#define MOVIE_H

#include <QtGui>

/**
 * The encoded bytes of an animation and, once load() has run, the QMovie that
 * plays them. load() deliberately waits until the caller that will display the
 * animation runs, because that is the thread the QMovie belongs to. Copies share
 * the bytes and the reader, so a Movie behaves as a value.
 */
class Movie
{
public:
    Movie() = default;
    Movie(QByteArray bytes, QString format);

    /** Creates the reader. Repeated calls keep the reader that already exists. */
    void load();

    QMovie *data() { return m_movie.data(); }
    /** True when no bytes were ever set, which is how a default Movie reads. */
    bool isNull() const { return m_bytes.isNull(); }

private:
    QSharedPointer<QMovie> m_movie;
    QSharedPointer<QBuffer> m_buffer;
    QSharedPointer<QByteArray> m_bytes;
    QString m_format;
};

#endif // MOVIE_H
