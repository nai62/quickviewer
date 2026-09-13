#ifndef PAGENAVIGATOR_H
#define PAGENAVIGATOR_H

class PageNavigator
{
public:
    /** Returns the zero-based index of the first visible page. */
    int currentPageIndex() const { return m_currentPageIndex; }

    bool selectPage(int pageIndex, int pageCount)
    {
        if (pageIndex < 0 || pageIndex >= pageCount) {
            return false;
        }
        m_currentPageIndex = pageIndex;
        return true;
    }

    void reset(int pageIndex = 0) { m_currentPageIndex = pageIndex; }

private:
    int m_currentPageIndex = 0;
};

#endif // PAGENAVIGATOR_H
