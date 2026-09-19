#include "movie.h"

Movie::Movie(QByteArray bytes, QString format)
    : m_format(format)
{
    m_bytes.reset(new QByteArray(bytes));
}

void Movie::load()
{
    if (m_movie || !m_bytes) {
        return;
    }
    m_buffer.reset(new QBuffer(m_bytes.data()));
    // QMovie reads its device, so the buffer has to be open before the reader is
    // built; without that the first jumpToFrame() fails.
    m_buffer->open(QIODevice::ReadOnly);
    m_movie.reset(new QMovie(m_buffer.data(), m_format.toUtf8()));
}
