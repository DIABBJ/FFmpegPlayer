#ifndef DEMUX_H
#define DEMUX_H

#include <QObject>
#include <QDebug>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
extern "C"
{
//avcodec:编解码(最重要的库)
#include <libavcodec/avcodec.h>
//avformat:封装格式处理
#include <libavformat/avformat.h>
//avutil:工具库（大部分库都需要这个库的支持）
#include <libavutil/avutil.h>
//error错误处理
#include <libavutil/error.h>
}
class Demux : public QObject
{
    Q_OBJECT
public:
    explicit Demux(QQueue<AVPacket*> &audioPacketQueue,QQueue<AVPacket*> &videoPacketQueue,
                   QMutex &audioPacketMutex,QMutex &videoPacketMutex,
                   QWaitCondition &audioPacketCond,QWaitCondition &videoPacketCond,
                   qint64 &totalPauseDuration,QElapsedTimer &timer,bool &isDemux,bool &isSeek,
                   QObject *parent = nullptr);
    ~Demux();

private:
    //共享资源
    QQueue<AVPacket*> &audioPacketQueue;
    QQueue<AVPacket*> &videoPacketQueue;
    QMutex &audioPacketMutex;
    QMutex &videoPacketMutex;
    QWaitCondition &audioPacketCond;
    QWaitCondition &videoPacketCond;
    qint64 &totalPauseDuration;
    QElapsedTimer &timer;
    bool &isDemux;
    bool &isSeek;

    AVFormatContext *formatCtx;
    AVStream *audioStream;
    AVStream *videoStream;
    AVCodecParameters *audioCodecParams;
    AVCodecParameters *videoCodecParams;
    int audioStreamIndex=-1;   //音频和视频流对应编号从0开始，-1即没找到，故初始化-1
    int videoStreamIndex=-1;
    void work();

signals:
    void durationSet(qint64 duration);
    void audioDecodeStart(AVCodecParameters *audioCodecParams);
    void videoDecodeStart(AVCodecParameters *videoCodecParams);
    void audioPlayStart(AVStream *audioStream);
    void videoPlayStart(AVStream *videoStream);
    void seekFinish();

public slots:
    void init(QString filePath);
    void seek(qint64 position);
};

#endif // DEMUX_H
