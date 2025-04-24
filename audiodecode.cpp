#include "audiodecode.h"

AudioDecode::AudioDecode(QQueue<AVPacket*> &audioPacketQueue,QQueue<AVFrame*> &audioFrameQueue,
                         QMutex &audioPacketMutex,QMutex &audioFrameMutex,
                         QWaitCondition &audioPacketCond,QWaitCondition &audioFrameCond,
                         bool &isClose,bool &isSeek,
                         QObject *parent)
    : QObject{parent},
    audioPacketQueue(audioPacketQueue),audioFrameQueue(audioFrameQueue),
    audioPacketMutex(audioPacketMutex),audioFrameMutex(audioFrameMutex),
    audioPacketCond(audioPacketCond),audioFrameCond(audioFrameCond),
    isClose(isClose),isSeek(isSeek)
{}

AudioDecode::~AudioDecode()
{
    if(audioCodecCtx)
        avcodec_free_context(&audioCodecCtx);
    qDebug()<<"audioDecode退出了";
}

void AudioDecode::init(AVCodecParameters *audioCodecParams)
{
    //查找并打开音频解码器
    audioCodec=avcodec_find_decoder(audioCodecParams->codec_id);
    if(!audioCodec){
        qDebug()<<"找不到音频解码器";
        return;
    }

    audioCodecCtx=avcodec_alloc_context3(audioCodec);  //audioCodecCtx是解码器的上下文，包含了关于解码器的所有设置和状态信息
    if(!audioCodecCtx){
        qDebug()<<"无法分配音频解码器上下文";
        return;
    }

    if(avcodec_parameters_to_context(audioCodecCtx,audioCodecParams)<0){
        qDebug()<<"无法初始化音频解码器上下文";
        return;
    }

    if(avcodec_open2(audioCodecCtx,audioCodec,nullptr)<0){  //audioCodec是音频解码器
        qDebug()<<"无法打开音频解码器";
        return;
    }
    decodeProcess();
}

void AudioDecode::decodeProcess()
{
    int ret=0;
    AVPacket *packet=nullptr;
    AVFrame *frame=av_frame_alloc(); //初始化分配内存
    while(!isClose){
        if(isSeek){
            {
                QMutexLocker locker(&audioPacketMutex);
                while(!audioPacketQueue.isEmpty()&&!isClose){
                    packet=audioPacketQueue.dequeue();
                    av_packet_free(&packet);
                }
            }
            if(audioCodecCtx)
                avcodec_flush_buffers(audioCodecCtx);
        }
        {
            //加锁以确保线程安全
            QMutexLocker locker(&audioPacketMutex);
            //当audioPacketQueue空时会释放互斥锁audioPacketMutex，当前线程进入等待状态，直到条件变量被其他线程发出信号
            while((audioPacketQueue.isEmpty()||audioFrameQueue.size()>10)&&!isClose){
                audioPacketCond.wait(&audioPacketMutex,15);
            }
            if(isClose) break;
            //从队列中取出音频数据包
            packet=audioPacketQueue.dequeue();
        }
        qDebug()<<"音频帧队列大小:"<<audioFrameQueue.size();
        //解码音频数据包
        ret=avcodec_send_packet(audioCodecCtx,packet);  //将数据包发送到解码器上下文，这个操作不会立即解码数据包
        if(ret<0){
            qDebug()<<"发送音频数据包到音频解码器上下文失败";
            continue;
        }
        ret=avcodec_receive_frame(audioCodecCtx,frame);  //从解码器获取解码后的音频帧
        if(ret==AVERROR(EAGAIN)){  //解码器需要更多的输入数据
            continue;
        }
        else if(ret<0){
            qDebug()<<"接收音频解码帧失败";
            continue;
        }
        {
            //加锁以确保线程安全
            QMutexLocker locker(&audioFrameMutex);
            audioFrameQueue.enqueue(av_frame_clone(frame));  //复制一个frame加入队列
            audioFrameCond.wakeOne();  //唤醒等待音频帧队列的线程
        }
        av_packet_free(&packet);  //释放AVPacket包
    }
    av_frame_free(&frame); //释放AVFrame帧
}
