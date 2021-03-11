#ifndef QLABELWITHMOUSEEVENTS_HPP
#define QLABELWITHMOUSEEVENTS_HPP

#include <QLabel>

class QLabelWithMouseEvents : public QLabel
{
public:
    QLabelWithMouseEvents(QWidget* parent) : QLabel(parent)
    {
        standardColor_ = palette().color(QPalette::Background).rgb();
        setAutoFillBackground(true);
    }

    bool isPressed() const
    { return pressed_; }
    void setNotPressed()
    {
        pressed_ = false;
        setBackgroundColor(standardColor_);
    }

protected:
    void enterEvent(QEvent* event) override
    {
        QLabel::enterEvent(event);
        if (isPressed())
        { return; }
        setBackgroundColor(hoverColor_);
    }
    void leaveEvent(QEvent* event) override
    {
        QLabel::leaveEvent(event);
        if (isPressed())
        { return; }
        setBackgroundColor(standardColor_);
    }
    void mousePressEvent(QMouseEvent* event) override
    {
        QLabel::mousePressEvent(event);
        if (isPressed())
        { return; }
        pressed_ = true;
        setBackgroundColor(pressedColor_);
    }

private:
    void setBackgroundColor(QRgb color)
    {
        auto p = palette();
        p.setColor(QPalette::Background, color);
        setPalette(p);
    }

    QRgb standardColor_;
    QRgb hoverColor_{qRgb(0xff, 0xff, 0x00)};
    QRgb pressedColor_{qRgb(0x7f, 0xff, 0x00)};
    bool pressed_{false};
};

#endif // QLABELWITHMOUSEEVENTS_HPP
