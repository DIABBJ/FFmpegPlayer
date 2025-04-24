#ifndef VIDEOPLAY_H
#define VIDEOPLAY_H

#include <QObject>
#include <QDebug>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QLabel>
#include <QCoreApplication>
extern "C"
{
//swscale:视频像素数据格式转换
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
//avcodec:编解码(最重要的库)
#include <libavcodec/avcodec.h>
}
class VideoPlay : public QObject
{
    Q_OBJECT
public:
    explicit VideoPlay(QQueue<AVFrame*> &videoFrameQueue,QMutex &videoFrameMutex,QWaitCondition &videoFrameCond,
                       qint64 &videoFirstFramePts,qint64 &totalPauseDuration,QElapsedTimer &timer,bool &isClose,bool &isSeek,bool &isPause,double &playSpeed,QLabel *videoLabel,
                       QObject *parent = nullptr);
    ~VideoPlay();

private:
    //共享资源
    QQueue<AVFrame*> &videoFrameQueue;
    QMutex &videoFrameMutex;
    QWaitCondition &videoFrameCond;
    qint64 &videoFirstFramePts;
    qint64 &totalPauseDuration;
    QElapsedTimer &timer;
    bool &isClose;
    bool &isSeek;
    bool &isPause;
    double &playSpeed;
    QLabel *videoLabel;

    AVStream *videoStream;
    void work();

signals:

public slots:
    void init(AVStream *avStream);
};

#endif // VIDEOPLAY_H
