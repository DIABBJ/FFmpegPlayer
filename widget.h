#ifndef WIDGET_H
#define WIDGET_H

#include "demux.h"
#include "audiodecode.h"
#include "videodecode.h"
#include "audioplay.h"
#include "videoplay.h"
#include <QWidget>
#include <QThread>
#include <QFileDialog>
QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

    //共享资源
    QQueue<AVPacket*> audioPacketQueue;
    QQueue<AVFrame*> audioFrameQueue;
    QMutex audioPacketMutex;
    QMutex audioFrameMutex;
    QWaitCondition audioPacketCond;
    QWaitCondition audioFrameCond;
    qint64 audioFirstFramePts=-1;
    QAudioOutput *audioOutput=nullptr;

    QQueue<AVPacket*> videoPacketQueue;
    QQueue<AVFrame*> videoFrameQueue;
    QMutex videoPacketMutex;
    QMutex videoFrameMutex;
    QWaitCondition videoFrameCond;
    QWaitCondition videoPacketCond;
    qint64 videoFirstFramePts=-1;

    QElapsedTimer timer;   //QElapsedTimer是Qt提供的一个高精度计时器类
    bool isDemux=true;
    bool isClose=false;
    bool isSeek=false;
    bool isPause=false;
    qint64 totalPauseDuration=0;
    qint64 pauseStartTime=0;
    double playSpeed=1.0; //播放速度因子

signals:
    void demuxStart(QString fileName);
    void seek(qint64 position);

private slots:
    void on_openFileButton_clicked();
    void on_audioButton_clicked();
    void on_audioSlider_valueChanged(int value);
    void on_pauseButton_clicked();

    void on_playSpeed_currentIndexChanged(int index);

private:
    Demux *demux;
    AudioDecode *audioDecode;
    AudioPlay *audioPlay;
    QThread *audioDecodeThread;
    QThread *audioPlayThread;

    QThread *demuxThread;
    VideoDecode *videoDecode;
    VideoPlay *videoPlay;
    QThread *videoDecodeThread;
    QThread *videoPlayThread;

    float audioVolume=0.1;
    Ui::Widget *ui;
};
#endif // WIDGET_H
