#include "pageslider.h"
#include <QProxyStyle>

class PageStyle : public QProxyStyle
{
public:
    PageStyle(QStyle *style)
        : QProxyStyle(style)
    {}
    int styleHint(QStyle::StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const
    {
        if (hint == QStyle::SH_Slider_AbsoluteSetButtons) {
            return (Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

PageSlider::PageSlider(QWidget *parent)
    : QSlider(parent)
{
    setStyle(new PageStyle(style()));
}
