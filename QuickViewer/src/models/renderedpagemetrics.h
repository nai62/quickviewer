#ifndef RENDEREDPAGEMETRICS_H
#define RENDEREDPAGEMETRICS_H

#include <QtCore>

class RenderedPageMetrics
{
public:
    static constexpr int Capacity = 2;

    RenderedPageMetrics() = default;
    explicit RenderedPageMetrics(QVector<qreal> notationalScales)
        : m_notationalScales(std::move(notationalScales))
    {
        if(m_notationalScales.size() > Capacity)
            m_notationalScales.resize(Capacity);
    }

    int count() const { return m_notationalScales.size(); }
    bool isEmpty() const { return m_notationalScales.isEmpty(); }
    qreal notationalScaleAt(int index) const
    {
        return index >= 0 && index < count()
                ? m_notationalScales[index] : 1.0;
    }

private:
    QVector<qreal> m_notationalScales;
};

#endif // RENDEREDPAGEMETRICS_H
