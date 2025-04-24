#ifndef AUDIODECODE_H
#define AUDIODECODE_H

#include <QObject>
#include <QDebug>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
extern "C"
{
#include <libavcodec/avcodec.h>  //编解码库
}
class AudioDecode : public QObject
{
    Q_OBJECT
public:
    explicit AudioDecode(QQueue<AVPacket*> &audioPacketQueue,QQueue<AVFrame*> &audioFrameQueue,
                         QMutex &audioPacketMutex,QMutex &audioFrameMutex,
                         QWaitCondition &audioPacketCond,QWaitCondition &audioFrameCond,
                         bool &isClose,bool &isSeek,
                         QObject *parent = nullptr);
    ~AudioDecode();

private:
    //共享资源
    QQueue<AVPacket*> &audioPacketQueue;
    QQueue<AVFrame*> &audioFrameQueue;
    QMutex &audioPacketMutex;
    QMutex &audioFrameMutex;
    QWaitCondition &audioPacketCond;
    QWaitCondition &audioFrameCond;
    bool &isClose;
    bool &isSeek;

    const AVCodec *audioCodec;
    AVCodecContext *audioCodecCtx;
    void decodeProcess();

signals:

public slots:
    void init(AVCodecParameters *audioCodecParams);
};

#endif // AUDIODECODE_H
