#include "videodecode.h"

VideoDecode::VideoDecode(QQueue<AVPacket*> &videoPacketQueue,QQueue<AVFrame*> &videoFrameQueue,
                         QMutex &videoPacketMutex,QMutex &videoFrameMutex,
                         QWaitCondition &videoPacketCond,QWaitCondition &videoFrameCond,
                         bool &isClose,bool &isSeek,
                         QObject *parent)
    : QObject{parent},
    videoPacketQueue(videoPacketQueue),videoFrameQueue(videoFrameQueue),
    videoPacketMutex(videoPacketMutex),videoFrameMutex(videoFrameMutex),
    videoPacketCond(videoPacketCond),videoFrameCond(videoFrameCond),
    isClose(isClose),isSeek(isSeek)
{}

VideoDecode::~VideoDecode()
{
    if (videoCodecCtx) {
        avcodec_free_context(&videoCodecCtx);
    }
    qDebug()<<"videoDecode退出了";
}

void VideoDecode::init(AVCodecParameters *videoCodecParams)
{
    //查找并打开视频解码器
    videoCodec = avcodec_find_decoder(videoCodecParams->codec_id);
    if(!videoCodec){
        qDebug() << "找不到视频解码器";
        return;
    }

    videoCodecCtx = avcodec_alloc_context3(videoCodec);
    if(!videoCodecCtx){
        qDebug() << "无法分配视频解码器上下文";
        return;
    }

    if(avcodec_parameters_to_context(videoCodecCtx, videoCodecParams)<0) {
        qDebug() << "无法初始化视频解码器上下文";
        return;
    }

    if(avcodec_open2(videoCodecCtx,videoCodec,nullptr)<0){
        qDebug() << "无法打开视频解码器";
        return;
    }
    decodeProcess();
}

void VideoDecode::decodeProcess()
{
    int ret=0;
    AVPacket *packet=nullptr;
    AVFrame *frame=av_frame_alloc(); //初始化分配内存

    
    while(!isClose){
        if(isSeek){
            {
                QMutexLocker locker(&videoPacketMutex);
                while(!videoPacketQueue.isEmpty()&&!isClose){
                    packet=videoPacketQueue.dequeue();
                    av_packet_free(&packet);
                }
            }
            if(videoCodecCtx)
                avcodec_flush_buffers(videoCodecCtx);
        }
        {
            //加锁以确保线程安全
            QMutexLocker locker(&videoPacketMutex);
            //当videoPacketQueue空时会释放互斥锁videoPacketMutex，当前线程进入等待状态，直到条件变量被其他线程发出信号
            while((videoPacketQueue.isEmpty()||videoFrameQueue.size()>10)&&!isClose){
                videoPacketCond.wait(&videoPacketMutex,15);
            }
            if(isClose) break;
            //从队列中取出视频数据包
            packet=videoPacketQueue.dequeue();
        }
        qDebug()<<"视频帧队列大小:"<<videoFrameQueue.size();
        //解码视频数据包
        ret=avcodec_send_packet(videoCodecCtx,packet);  //将数据包发送到解码器上下文，这个操作不会立即解码数据包
        if(ret<0){
            qDebug()<<"发送视频数据包到视频解码器上下文失败";
            continue;
        }

        ret=avcodec_receive_frame(videoCodecCtx,frame);  //从解码器获取解码后的视频帧
        if(ret==AVERROR(EAGAIN)){  //解码器需要更多的输入数据
            continue;
        }
        else if(ret<0){
            qDebug()<<"接收视频解码帧失败";
            continue;
        }

        {
            //加锁以确保线程安全
            QMutexLocker locker(&videoFrameMutex);
            videoFrameQueue.enqueue(av_frame_clone(frame));  //复制一个frame加入队列
            videoFrameCond.wakeOne();  //唤醒等待视频帧队列的线程
        }
        av_packet_free(&packet);  //释放AVPacket包
    }
    av_frame_free(&frame); //释放AVFrame帧
}
