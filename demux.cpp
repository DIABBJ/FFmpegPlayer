#include "demux.h"

Demux::Demux(QQueue<AVPacket*> &audioPacketQueue,QQueue<AVPacket*> &videoPacketQueue,
             QMutex &audioPacketMutex,QMutex &videoPacketMutex,
             QWaitCondition &audioPacketCond,QWaitCondition &videoPacketCond,
             qint64 &totalPauseDuration,QElapsedTimer &timer,bool &isDemux,bool &isSeek,
             QObject  *parent)
    : QObject{parent},
    audioPacketQueue(audioPacketQueue),videoPacketQueue(videoPacketQueue),
    audioPacketMutex(audioPacketMutex),videoPacketMutex(videoPacketMutex),
    audioPacketCond(audioPacketCond),videoPacketCond(videoPacketCond),
    totalPauseDuration(totalPauseDuration),timer(timer),isDemux(isDemux),isSeek(isSeek)
{}

Demux::~Demux()
{
    //释放相关资源
    avformat_close_input(&formatCtx);
}

void Demux::init(QString filePath)
{
    formatCtx=avformat_alloc_context(); //AVFormatContext是一个保存输入或输出多媒体文件的格式相关信息的结构体,它包含了处理多媒体文件（如视频文件、音频文件或流媒体）所需的所有信息和状态。
    int ret=0;

    ret=avformat_open_input(&formatCtx, filePath.toStdString().c_str(), nullptr, nullptr);
    if (ret<0) {
        qDebug()<< "无法打开视频文件";
        return;
    }

    ret=avformat_find_stream_info(formatCtx,nullptr);
    if(ret<0){
        qDebug() << "无法获取流信息";
        return;
    }

    audioStreamIndex=av_find_best_stream(formatCtx,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);  //寻找音频流
    videoStreamIndex=av_find_best_stream(formatCtx,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);  //寻找视频流

    if(audioStreamIndex<0||videoStreamIndex<0){  //只支持音频流和视频流都有的情况
        qDebug()<<"音频或视频流不存在";
        return;
    }

    //formatCtx->duration 是以 AV_TIME_BASE 为单位的,为了提供一个统一的、高精度的时间表示方法，适用于各种不同的媒体格式和编码方式,即全局时间基
    qint64 duration = (formatCtx->duration*1000)/AV_TIME_BASE; //转换为毫秒
    emit durationSet(duration);

    audioStream=formatCtx->streams[audioStreamIndex];  //获取音频流
    videoStream=formatCtx->streams[videoStreamIndex];  //获取视频流
    audioCodecParams=audioStream->codecpar;   //获取音频流的编解码器参数
    videoCodecParams=videoStream->codecpar;   //获取视频流的编解码器参数
    emit audioDecodeStart(audioCodecParams);
    emit videoDecodeStart(videoCodecParams);
    emit audioPlayStart(audioStream);
    emit videoPlayStart(videoStream);
    timer.start();

    work();
}

void Demux::work()
{
    AVPacket *packet=av_packet_alloc();  //分配一个新的AVPacket并将其字段初始化为默认值,返回一个指向新分配的AVPacket的指针
    timer.restart();
    totalPauseDuration=0;
    while(av_read_frame(formatCtx, packet)>=0&&isDemux){
        //qDebug()<<"音频包队列大小："<<audioPacketQueue.size();
        //qDebug()<<"视频包队列大小："<<videoPacketQueue.size();
        if(packet->stream_index==audioStreamIndex){
            QMutexLocker locker(&audioPacketMutex);
            audioPacketQueue.enqueue(av_packet_clone(packet));  //复制一个packet加入队列
            audioPacketCond.wakeOne();  //唤醒等待音频包队列的线程
        }
        else if(packet->stream_index==videoStreamIndex){
            QMutexLocker locker(&videoPacketMutex);
            videoPacketQueue.enqueue(av_packet_clone(packet));  //复制一个packet加入队列
            videoPacketCond.wakeOne();  //唤醒等待视频包队列的线程
        }
        while((audioPacketQueue.size()>10&&videoPacketQueue.size()>10)&&isDemux){
            QThread::msleep(50);
        }
    }
    av_packet_free(&packet);
}

void Demux::seek(qint64 position)
{
    if(!formatCtx)
        return;
    isSeek=true;
    // 将position（毫秒）转换为AV_TIME_BASE单位
    qint64 temp=position*AV_TIME_BASE/1000;

    //寻找最近的关键帧
    if(av_seek_frame(formatCtx,-1,temp,AVSEEK_FLAG_BACKWARD)<0){
        qDebug()<<"跳转失败";
        return;
    }
    //清空解码器缓存
    avformat_flush(formatCtx);
    QThread::msleep(50);
    isDemux=true;
    isSeek=false;
    work();
}
