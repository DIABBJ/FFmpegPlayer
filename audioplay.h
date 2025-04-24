#ifndef AUDIOPLAY_H
#define AUDIOPLAY_H

#include <QObject>
#include <QDebug>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QAudioOutput>
#include <QThread>
#include <QCoreApplication>

extern "C"
{
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
}
class AudioPlay : public QObject
{
    Q_OBJECT
public:
    explicit AudioPlay(QQueue<AVFrame*> &audioFrameQueue,QMutex &audioFrameMutex,QWaitCondition &audioFrameCond,
                       QAudioOutput *audioOutput,qint64 &audioFirstFramePts,qint64 &totalPauseDuration,
                       QElapsedTimer &timer,bool &isClose,bool &isSeek,bool &isPause,double &playSpeed,
                       QObject *parent = nullptr);
    ~AudioPlay();

private:
    //共享资源
    QQueue<AVFrame*> &audioFrameQueue;
    QMutex &audioFrameMutex;
    QWaitCondition &audioFrameCond;
    QAudioOutput *audioOutput;
    qint64 &audioFirstFramePts;
    qint64 &totalPauseDuration;
    QElapsedTimer &timer;
    bool &isClose;
    bool &isSeek;
    bool &isPause;
    double &playSpeed;

    AVStream *audioStream;
    QIODevice *audioDevice;   //音频输出设备
    SwrContext *swrContext;   //重采样上下文
    void work();

signals:
    void videoSliderUpdate(qint64 position);

public slots:
    void init(AVStream *avStream);
};

#endif // AUDIOPLAY_H
