#ifndef AGGRIDSCROLLBAR_H
#define AGGRIDSCROLLBAR_H

#include "AgGrid_global.h"

#include <QScrollBar>
#include <QPropertyAnimation>
#include <QTimer>

#include <QtGlobal>

class AGGRID_EXPORT AgGridScrollBar : public QScrollBar
{
    Q_OBJECT
    Q_PROPERTY(qreal sliderExtent READ sliderExtent WRITE setSliderExtent)
    Q_PROPERTY(qreal backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity)

public:
    explicit AgGridScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~AgGridScrollBar() override;

    QColor sliderColor() const;
    void setSliderColor(const QColor &color);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);

    QColor separatorColor() const;
    void setSeparatorColor(const QColor &color);

    int trackMargin() const;
    void setTrackMargin(int margin);

protected:
    void paintEvent(QPaintEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    qreal sliderExtent() const;
    void setSliderExtent(qreal extent);

    qreal backgroundOpacity() const;
    void setBackgroundOpacity(qreal opacity);

    QRect sliderRect() const;
    QRect trackRect() const;
    int pageScrollDirectionAt(const QPoint &pos) const;

    void startExpandAnimation(bool expand);
    void startRepeatTimer(int direction);
    void stopRepeatTimer();
    void onRepeatTimeout();

    QColor m_sliderColor;
    QColor m_backgroundColor;
    QColor m_separatorColor;
    qreal m_sliderExtent;
    qreal m_backgroundOpacity;
    bool m_isExpanded;
    bool m_isDragging;
    int m_dragStartValue;
    int m_dragStartPos;
    QPropertyAnimation *m_extentAnimation;
    QPropertyAnimation *m_opacityAnimation;
    QTimer *m_repeatTimer;
    int m_repeatDirection;
    int m_trackMargin;

    static constexpr qreal kCollapsedExtent = 4.0;
    static constexpr qreal kExpandedExtent = 10.0;
    static constexpr qreal kSliderMargin = 2.0;
    static constexpr int kMinSliderLength = 24;
};

#endif // AGGRIDSCROLLBAR_H
