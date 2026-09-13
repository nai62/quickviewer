#ifndef VISIBLEPAGES_H
#define VISIBLEPAGES_H

#include <utility>

#include <QVector>

#include "imagecontent.h"

class VisiblePages
{
public:
    static constexpr int Capacity = 2;

    VisiblePages() = default;
    explicit VisiblePages(QVector<ImageContent> pages)
        : m_pages(std::move(pages))
    {
        if (m_pages.size() > Capacity) {
            m_pages.resize(Capacity);
        }
    }

    bool isEmpty() const { return m_pages.isEmpty(); }
    int count() const { return m_pages.size(); }

    const ImageContent *at(int index) const
    {
        return index >= 0 && index < m_pages.size() ? &m_pages[index] : nullptr;
    }

    const ImageContent *first() const { return at(0); }

private:
    QVector<ImageContent> m_pages;
};

#endif // VISIBLEPAGES_H
