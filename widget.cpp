#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    ui->playSpeed->addItem("0.75x",0.75);
    ui->playSpeed->addItem("1.0x",1.0);
    ui->playSpeed->addItem("1.25x",1.25);
    ui->playSpeed->setCurrentIndex(1);

    //初始化音频播放设备的各种设置
    QAudioFormat format;
    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    format.setSampleRate(48000);   //设置采样率
    format.setChannelCount(2);   //设置声道数
    format.setSampleSize(16);   //设置设置采样大小，Qt的音频系统通常最好使用16位有符号整数，因为这是最广泛支持的格式
    format.setSampleType(QAudioFormat::SignedInt);  //设置采样类型
    format.setByteOrder(QAudioFormat::LittleEndian);  //大多数音频文件和系统使用小端序
    format.setCodec("audio/pcm");   //Qt的音频系统主要期望接收pcm数据
    audioOutput=new QAudioOutput(format,this);
    audioOutput->setVolume(audioVolume);
    ui->audioSlider->setValue(audioVolume*100);
    ui->audioButton->setIcon(QIcon(":/icon1/audioOn.png"));
    ui->pauseButton->setIcon(QIcon(":/icon1/pause.png"));
    //创建任务类对象
    demux=new Demux(audioPacketQueue,videoPacketQueue,audioPacketMutex,videoPacketMutex,audioPacketCond,videoPacketCond,totalPauseDuration,timer,isDemux,isSeek);
    audioDecode=new AudioDecode(audioPacketQueue,audioFrameQueue,audioPacketMutex,audioFrameMutex,audioPacketCond,audioFrameCond,isClose,isSeek);
    videoDecode=new VideoDecode(videoPacketQueue,videoFrameQueue,videoPacketMutex,videoFrameMutex,videoPacketCond,videoFrameCond,isClose,isSeek);
    audioPlay=new AudioPlay(audioFrameQueue,audioFrameMutex,audioFrameCond,audioOutput,audioFirstFramePts,totalPauseDuration,timer,isClose,isSeek,isPause,playSpeed);
    videoPlay=new VideoPlay(videoFrameQueue,videoFrameMutex,videoFrameCond,videoFirstFramePts,totalPauseDuration,timer,isClose,isSeek,isPause,playSpeed,ui->videoLabel);
    //创建子线程对象
    demuxThread=new QThread();
    audioDecodeThread=new QThread();
    videoDecodeThread=new QThread();
    audioPlayThread=new QThread();
    videoPlayThread=new QThread();
    //将任务类对象移动到子线程中
    demux->moveToThread(demuxThread);
    audioDecode->moveToThread(audioDecodeThread);
    videoDecode->moveToThread(videoDecodeThread);
    audioPlay->moveToThread(audioPlayThread);
    videoPlay->moveToThread(videoPlayThread);
    demuxThread->start();
    audioDecodeThread->start();
    videoDecodeThread->start();
    audioPlayThread->start();
    videoPlayThread->start();

    connect(demux,&Demux::durationSet,this,[this](qint64 duration){   //设置进度条总长度
        ui->videoSlider->setRange(0, duration);
        // 将时长转换为时分秒格式
        int hours = duration/1000/3600;
        int minutes = (duration/1000%3600)/60;
        int seconds = duration/1000%60;
        // 格式化时间字符串 (格式如 "00:00:00")
        QString timeStr = QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
        ui->durationLabel->setText(timeStr);
    });
    connect(audioPlay,&AudioPlay::videoSliderUpdate,this,[this](qint64 position){   //根据音频帧的播放来改变进度条滑块的位置
        ui->videoSlider->setValue(position);
        // 转换当前位置为时分秒
        int hours = position/1000/3600;
        int minutes = (position/1000%3600)/60;
        int seconds = position/1000%60;
        // 格式化当前时间
        QString timeStr = QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
        ui->progressLabel->setText(timeStr);
    });
    connect(ui->videoSlider,&VideoSlider::videoSliderChange,this,[this](){
        isDemux=false;
    });
    connect(ui->videoSlider,&VideoSlider::videoSliderChange,demux,&Demux::seek);
    connect(this,&Widget::demuxStart,demux,&Demux::init);   //解封装线程开始
    connect(demux,&Demux::audioDecodeStart,audioDecode,&AudioDecode::init); //音频解码开始
    connect(demux,&Demux::videoDecodeStart,videoDecode,&VideoDecode::init); //视频解码开始
    connect(demux,&Demux::audioPlayStart,audioPlay,&AudioPlay::init); //音频播放开始
    connect(demux,&Demux::videoPlayStart,videoPlay,&VideoPlay::init); //视频播放开始
    this->setFixedSize(this->size());
}

Widget::~Widget()
{
    isClose=true;
    isDemux=false;

    demuxThread->quit();
    demuxThread->wait();
    audioDecodeThread->quit();
    audioDecodeThread->wait();
    videoDecodeThread->quit();
    videoDecodeThread->wait();
    audioPlayThread->quit();
    audioPlayThread->wait();
    videoPlayThread->quit();
    videoPlayThread->wait();

    AVPacket *packet=nullptr;
    AVFrame *frame=nullptr;
    {
        QMutexLocker locker(&audioPacketMutex);
        while(!audioPacketQueue.isEmpty()){
            packet=audioPacketQueue.dequeue();
            av_packet_free(&packet);
        }
    }
    {
        QMutexLocker locker(&videoPacketMutex);
        while(!videoPacketQueue.isEmpty()){
            packet=videoPacketQueue.dequeue();
            av_packet_free(&packet);
        }
    }
    {
        QMutexLocker locker(&audioFrameMutex);
        while(!audioFrameQueue.isEmpty()){
            frame=audioFrameQueue.dequeue();
            av_frame_free(&frame);
        }
    }
    {
        QMutexLocker locker(&videoFrameMutex);
        while(!videoFrameQueue.isEmpty()){
            frame=videoFrameQueue.dequeue();
            av_frame_free(&frame);
        }
    }

    //删除对象
    if(demux)
        demux->deleteLater();
    if(audioDecode)
        audioDecode->deleteLater();
    if(videoDecode)
        videoDecode->deleteLater();
    if(audioPlay)
        audioPlay->deleteLater();
    if(videoPlay)
        videoPlay->deleteLater();
    if(demuxThread)
        demuxThread->deleteLater();
    if(audioDecodeThread)
        audioDecodeThread->deleteLater();
    if(videoDecodeThread)
        videoDecodeThread->deleteLater();
    if(audioPlayThread)
        audioPlayThread->deleteLater();
    if(videoPlayThread)
        videoPlayThread->deleteLater();
}

void Widget::on_openFileButton_clicked()
{
    QString filePath=QFileDialog::getOpenFileName(this, "选择一个视频文件", "", "视频文件(*.mp4 *.avi *.wmv *.mov)");
    if(filePath.isEmpty()){
        qDebug()<<"未选择视频文件";
        return;
    }
    emit demuxStart(filePath);
}

void Widget::on_audioButton_clicked()
{
    if(audioOutput){
        if(audioOutput->volume()>0) {
            audioOutput->setVolume(0);
            ui->audioButton->setIcon(QIcon(":/icon1/audioOff.png"));
        }
        else{
            audioOutput->setVolume(audioVolume);
            ui->audioButton->setIcon(QIcon(":/icon1/audioOn.png"));
        }
    }
}

void Widget::on_audioSlider_valueChanged(int value)
{
    if(audioOutput){
        audioOutput->setVolume(value/100.0);
        audioVolume=audioOutput->volume();
    }
}

void Widget::on_pauseButton_clicked()
{
    if(isPause){
        isPause=false;
        //计算这次暂停的时长，加到总暂停时长中
        totalPauseDuration=totalPauseDuration+(timer.elapsed()-pauseStartTime);
        ui->pauseButton->setIcon(QIcon(":/icon1/pause.png"));
    }
    else{
        isPause=true;
        //记录开始暂停的时间点
        pauseStartTime=timer.elapsed();
        ui->pauseButton->setIcon(QIcon(":/icon1/play.png"));
    }
}

void Widget::on_playSpeed_currentIndexChanged(int index)
{
    playSpeed=ui->playSpeed->itemData(index).toDouble();
}

