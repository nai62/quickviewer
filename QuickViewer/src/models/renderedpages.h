#ifndef RENDEREDPAGES_H
#define RENDEREDPAGES_H

#include <array>
#include <memory>
#include <utility>

#include "pagecontent.h"

class RenderedPages
{
public:
    static constexpr int Capacity = 2;

    int count() const
    {
        if(m_pages[1])
            return 2;
        return m_pages[0] ? 1 : 0;
    }

    bool add(std::unique_ptr<PageItem> page, bool append)
    {
        const int pageCount = count();
        if(!page || pageCount >= Capacity)
            return false;

        if(append) {
            m_pages[pageCount] = std::move(page);
        } else {
            if(pageCount == 1)
                m_pages[1] = std::move(m_pages[0]);
            m_pages[0] = std::move(page);
        }
        return true;
    }

    void clear()
    {
        m_pages[1].reset();
        m_pages[0].reset();
    }

    PageItem *at(int index)
    {
        return index >= 0 && index < count() ? m_pages[index].get() : nullptr;
    }

    const PageItem *at(int index) const
    {
        return index >= 0 && index < count() ? m_pages[index].get() : nullptr;
    }

    PageItem *first() { return at(0); }
    const PageItem *first() const { return at(0); }

    template<typename Function>
    void forEach(Function &&function)
    {
        const int pageCount = count();
        for(int index = 0; index < pageCount; ++index)
            function(*m_pages[index], index);
    }

private:
    std::array<std::unique_ptr<PageItem>, Capacity> m_pages;
};

#endif // RENDEREDPAGES_H
