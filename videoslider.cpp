#include "videoslider.h"

#include <QStyle>
VideoSlider::VideoSlider(QWidget *parent) : QSlider(parent)
{}

void VideoSlider::mousePressEvent(QMouseEvent *event)
{
    qint64 position = QStyle::sliderValueFromPosition(minimum(), maximum(), event->pos().x(), width());
    setValue(position);
    emit videoSliderChange(position);
}
