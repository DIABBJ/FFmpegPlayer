#ifndef VIDEOSLIDER_H
#define VIDEOSLIDER_H

#include <QSlider>
#include <QMouseEvent>

class VideoSlider : public QSlider
{
    Q_OBJECT

public:
    explicit VideoSlider(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void videoSliderChange(qint64 position);
};

#endif // VIDEOSLIDER_H
