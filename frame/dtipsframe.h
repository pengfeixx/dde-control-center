#ifndef DTIPSFRAME_H
#define DTIPSFRAME_H

#include <QLabel>
#include <QPropertyAnimation>

class DTipsFrame : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(ArrowDirection arrowDirection READ arrowDirection WRITE setArrowDirection)

    Q_ENUMS(ArrowDirection)
public:
    enum ArrowDirection {
        ArrowLeft,
        ArrowRight,
        ArrowTop,
        ArrowBottom
    };

    DTipsFrame();
    ~DTipsFrame();

    void move(int x, int y);
    ArrowDirection arrowDirection() const;

private:
    void updateStyle();
public slots:
    void followTheSender();
    void setTipsText(const QString &text);
    void show();
    void setArrowDirection(ArrowDirection arrowDirection);

private:
    QLabel *m_label;

    QPropertyAnimation *m_moveAni;

    bool m_isFirstMove = true;

    const int m_extraOffsetX = -8;
    const int m_extraOffsetY = 0;
    ArrowDirection m_arrowDirection = ArrowRight;
};

#endif // DTIPSFRAME_H
