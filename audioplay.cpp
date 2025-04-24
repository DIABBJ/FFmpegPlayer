#include "audioplay.h"

AudioPlay::AudioPlay(QQueue<AVFrame*> &audioFrameQueue,QMutex &audioFrameMutex, QWaitCondition &audioFrameCond,
                     QAudioOutput *audioOutput,qint64 &audioFirstFramePts,qint64 &totalPauseDuration,
                     QElapsedTimer &timer,bool &isClose,bool &isSeek,bool &isPause,double &playSpeed,
                     QObject *parent)
    : QObject{parent},
    audioFrameQueue(audioFrameQueue),audioFrameMutex(audioFrameMutex),audioFrameCond(audioFrameCond),audioOutput(audioOutput),
    audioFirstFramePts(audioFirstFramePts),totalPauseDuration(totalPauseDuration),timer(timer),isClose(isClose),isSeek(isSeek),isPause(isPause),playSpeed(playSpeed)
{}

AudioPlay::~AudioPlay()
{
    if(swrContext){
        swr_free(&swrContext);
    }
}

void AudioPlay::init(AVStream *avStream)
{
    audioStream=avStream;
    audioDevice=audioOutput->start();

    //重采样上下文
    swrContext=swr_alloc();

    // 输入音频参数
    int in_sample_rate = audioStream->codecpar->sample_rate;
    AVChannelLayout in_ch_layout = audioStream->codecpar->ch_layout;
    AVSampleFormat in_sample_fmt = (AVSampleFormat)audioStream->codecpar->format;
    // 输出音频参数
    int out_sample_rate = audioOutput->format().sampleRate();
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; //输出2声道为立体声
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; //输出为16位有符号整数

    swr_alloc_set_opts2(&swrContext,&out_ch_layout,out_sample_fmt,out_sample_rate,&in_ch_layout,in_sample_fmt,in_sample_rate,0, nullptr);

    if(swr_init(swrContext)<0) {
        qDebug()<<"无法初始化重采样上下文";
        swr_free(&swrContext);
        return;
    }

    work();
}

void AudioPlay::work(){
    AVFrame *frame=nullptr; //初始化分配内存
    while(!isClose){
        if(isPause){
            QThread::msleep(15);
            continue;
        }
        if(isSeek){
            {
                QMutexLocker locker(&audioFrameMutex);
                while(!audioFrameQueue.isEmpty()){
                    frame=audioFrameQueue.dequeue();
                    av_frame_free(&frame);
                }
            }
            if(audioDevice){
                audioDevice->reset();
            }
            if(swrContext){
                swr_close(swrContext);
                swr_init(swrContext);
            }
            audioFirstFramePts=-1;
        }
        {
            //加锁以确保线程安全
            QMutexLocker locker(&audioFrameMutex);
            //当audioFrameQueue空时会释放互斥锁audioFrameMutex，当前线程进入等待状态，直到条件变量被其他线程发出信号
            while(audioFrameQueue.isEmpty()&&!isClose){
                audioFrameCond.wait(&audioFrameMutex);
            }
            if(isClose) break;
            //从队列中取出音频数据帧
            frame=audioFrameQueue.dequeue();
        }

        qint64 pts=av_rescale_q(frame->pts,audioStream->time_base,(AVRational){1, 1000});

        if(audioFirstFramePts==-1){  //处理第一帧
            audioFirstFramePts=pts;
        }
        qint64 playTime=(timer.elapsed()-totalPauseDuration)*playSpeed+audioFirstFramePts;

        if(playTime<pts){
            QThread::msleep((pts-playTime)/playSpeed);   //如果播放时间落后于pts，则等待
        }
        else if(playTime>pts+(100*playSpeed)){ //100ms的容忍度
            av_frame_free(&frame);
            qDebug()<<"音频解码慢了";
            continue; //如果播放时间超前太多，则丢弃这一帧
        }

        //计算输出采样数
        int out_samples = av_rescale_rnd(swr_get_delay(swrContext,frame->sample_rate)+frame->nb_samples,audioOutput->format().sampleRate(),frame->sample_rate,AV_ROUND_UP);

        // 分配输出缓冲区
        uint8_t **outBuffer=nullptr;
        int outLinesize;
        av_samples_alloc_array_and_samples(&outBuffer,&outLinesize,audioOutput->format().channelCount(),out_samples,AV_SAMPLE_FMT_S16,0);

        // 执行重采样
        int samples_out=swr_convert(swrContext,outBuffer,out_samples,(const uint8_t **)frame->data,frame->nb_samples);

        if(samples_out>0){
            int buffer_size=av_samples_get_buffer_size(&outLinesize,audioOutput->format().channelCount(),samples_out,AV_SAMPLE_FMT_S16,1);

            qint64 bytesWritten = audioDevice->write((const char*)outBuffer[0], buffer_size);  //写入

            if(bytesWritten<0){
                qDebug()<<"写入错误";
            }

            emit videoSliderUpdate(pts);
        }
        //清理
        av_freep(&outBuffer[0]);
        av_freep(&outBuffer);
        av_frame_free(&frame);  //释放AVPacket帧
    }
    qDebug()<<"audioplay结束";
}
