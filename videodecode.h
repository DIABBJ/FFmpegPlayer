#ifndef VIDEODECODE_H
#define VIDEODECODE_H

#include <QObject>
#include <QDebug>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
extern "C"
{
#include <libavcodec/avcodec.h>  //编解码库
}
class VideoDecode : public QObject
{
    Q_OBJECT
public:
    explicit VideoDecode(QQueue<AVPacket*> &videoPacketQueue,QQueue<AVFrame*> &videoFrameQueue,
                         QMutex &videoPacketMutex,QMutex &videoFrameMutex,
                         QWaitCondition &videoPacketCond,QWaitCondition &videoFrameCond,
                         bool &isClose,bool &isSeek,
                         QObject *parent = nullptr);
    ~VideoDecode();

private:
    //共享资源
    QQueue<AVPacket*> &videoPacketQueue;
    QQueue<AVFrame*> &videoFrameQueue;
    QMutex &videoPacketMutex;
    QMutex &videoFrameMutex;
    QWaitCondition &videoPacketCond;
    QWaitCondition &videoFrameCond;
    bool &isClose;
    bool &isSeek;

    const AVCodec *videoCodec;
    AVCodecContext *videoCodecCtx;
    void decodeProcess();

signals:

public slots:
    void init(AVCodecParameters *videoCodecParams);
};

#endif // VIDEODECODE_H
