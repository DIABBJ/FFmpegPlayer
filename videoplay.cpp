#include "videoplay.h"

VideoPlay::VideoPlay(QQueue<AVFrame*> &videoFrameQueue,QMutex &videoFrameMutex,QWaitCondition &videoFrameCond,
                     qint64 &videoFirstFramePts,qint64 &totalPauseDuration,QElapsedTimer &timer,bool &isClose,bool &isSeek,bool &isPause,double &playSpeed,QLabel *videoLabel,
                     QObject *parent)
    : QObject{parent},
    videoFrameQueue(videoFrameQueue),videoFrameMutex(videoFrameMutex),videoFrameCond(videoFrameCond),
    videoFirstFramePts(videoFirstFramePts),totalPauseDuration(totalPauseDuration),timer(timer),isClose(isClose),isSeek(isSeek),isPause(isPause),playSpeed(playSpeed),videoLabel(videoLabel)
{}

VideoPlay::~VideoPlay()
{}

void VideoPlay::init(AVStream *avStream)
{
    videoStream=avStream;
    work();
}

void VideoPlay::work()
{
    AVFrame *frame=nullptr; //初始化分配内存
    SwsContext *swsCtx=nullptr;
    QImage *image=nullptr;
    QSize lastLabelSize; // 添加变量跟踪标签大小
    QPixmap scaledPixmap; // 缓存缩放后的图像

    while(!isClose){
        if(isPause){
            QThread::msleep(15);
            continue;
        }
        if(isSeek){
            {
                QMutexLocker locker(&videoFrameMutex);
                while(!videoFrameQueue.isEmpty()){
                    frame=videoFrameQueue.dequeue();
                    av_frame_free(&frame);
                }
            }
            if(videoLabel){
                videoLabel->clear();
            }
            videoFirstFramePts = -1;
        }
        {
            //加锁以确保线程安全
            QMutexLocker locker(&videoFrameMutex);
            //当videoFrameQueue空时会释放互斥锁videoFrameMutex，当前线程进入等待状态，直到条件变量被其他线程发出信号
            while(videoFrameQueue.isEmpty()&&!isClose){
                videoFrameCond.wait(&videoFrameMutex);
            }
            if(isClose) break;
            //从队列中取出视频数据帧
            frame=videoFrameQueue.dequeue();
        }

        qint64 pts = av_rescale_q(frame->pts,videoStream->time_base, (AVRational){1, 1000});

        if(videoFirstFramePts==-1){  //处理第一帧
            videoFirstFramePts=pts;
        }

        qint64 playTime=(timer.elapsed()-totalPauseDuration)*playSpeed+videoFirstFramePts;

        if(playTime<pts){
            QThread::msleep((pts-playTime)/playSpeed);   //如果播放时间落后于pts，则等待
        }
        else if(playTime>pts+(100*playSpeed)){ //100ms的容忍度
            av_frame_free(&frame); //如果播放时间超前太多，则丢弃这一帧
            qDebug()<<"视频解码慢了";
            continue;
        }

        // 只在标签尺寸改变时才重新计算缩放
        QSize currentSize = videoLabel->size();
        if (currentSize != lastLabelSize) {
            lastLabelSize = currentSize;
            scaledPixmap = QPixmap(currentSize);
        }

        if(!swsCtx) {
            // 直接转换为与标签大小相匹配的尺寸，减少一次缩放操作
            swsCtx = sws_getContext(
                frame->width, frame->height, (AVPixelFormat)frame->format,
                lastLabelSize.width(), lastLabelSize.height(), AV_PIX_FMT_RGB32,
                SWS_BICUBIC, NULL, NULL, NULL); // 使用更高质量的缩放算法
        }

        if(!image){
            image = new QImage(lastLabelSize.width(), lastLabelSize.height(), QImage::Format_RGB32);
        }

        uint8_t *destData[4] = {image->bits(), NULL, NULL, NULL};
        int destLinesize[4] = {image->bytesPerLine(), 0, 0, 0};

        //转换图像格式
        sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, destData, destLinesize);

        // 直接设置像素图，避免额外的缩放操作
        videoLabel->setPixmap(QPixmap::fromImage(*image));

        av_frame_free(&frame);  //释放AVFrame帧
    }
    //释放SwsContext
    sws_freeContext(swsCtx);
    if(image) {
        delete image;
        image = nullptr;
    }
    qDebug()<<"videplay结束";
}
