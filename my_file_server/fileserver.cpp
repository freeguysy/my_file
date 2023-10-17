#include "fileserver.h"
#include "ui_fileserver.h"

#include <QDataStream>
#include <QFileDialog>
#include <QFileInfo>
#include <QThread>

FileServer::FileServer(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::FileServer)
{
    ui->setupUi(this);
    m_tcpServer = new QTcpServer(this);
    connectSigSlots();
}

FileServer::~FileServer()
{
    delete ui;
}

//注册信号与槽函数
void FileServer::connectSigSlots()
{
    connect(ui->clearMsg,&QPushButton::clicked,[=](){
        ui->textBrowser->clear();
    });

    connect(ui->rebuildPushButton,&QPushButton::clicked,[=](){
        ui->sendProgressBar->setValue(0);
    });

    connect(ui->startServer,&QPushButton::clicked,[=](){
        if(ui->startServer->text() == "启动服务"){
            if(!startServer()){
                ui->textBrowser->append("服务没有正常启动");
                return ;
            }
            ui->startServer->setText("关闭服务");
        }else if(ui->startServer->text() == "关闭服务"){
            m_tcpServer->close();
            ui->startServer->setText("启动服务");
            ui->textBrowser->append(QString("服务器关闭成功、、、、、、"));
        }
    });

    connect(ui->selectFileBtn,&QPushButton::clicked,[=](){
        //打开文件目录
        QString filepath = QFileDialog::getOpenFileName(this, "选择文件", "./");
        if(filepath == "") return ;
        ui->fileEdit->setText(filepath);
        QFile file(filepath);
        QString Msg;
        if(file.size() > 1024*1024){
            Msg = QString("大小为：%1M").arg(file.size()/1024/1024.0);
        }else{
            Msg = QString("大小为：%1KB").arg(file.size()/1024);
        }
        ui->fileSizeLabel->setText(Msg);
    });
}

//注册监听
bool FileServer::startServer()
{
    if(!checkFile(ui->fileEdit->text())){
        return false;
    }
    ui->textBrowser->append(QString("服务器已经启动，端口号为%1").arg(ui->portEdit->text().toInt()));

    m_tcpServer->listen(QHostAddress::Any,ui->portEdit->text().toInt());

    connect(m_tcpServer,&QTcpServer::newConnection,this,&FileServer::delNewConnect);
    return true;

}

//创建新连接
void FileServer::delNewConnect()
{
    //队列中获取下一个待处理的连接。
    QTcpSocket* scoket = m_tcpServer->nextPendingConnection();
    //获取与当前套接字连接的对方的 IP 地址。
    ui->textBrowser->append(QString("客户端：[%1] 已连接...").arg(scoket->peerAddress().toString()));

    connect(scoket,&QTcpSocket::readyRead,[=](){
        dealMsg(scoket);
    });
}

//分析信息
void FileServer::dealMsg(QTcpSocket *socket)
{
    QDataStream in(socket);
    int typeMsg;
    in >> typeMsg;
    ui->textBrowser->append(QString("服务器收到客户端发来的消息: %1").arg(typeMsg));

    if(typeMsg == MsgType::FileInfo){
        transferFileInfo(socket);
    }else if(typeMsg == MsgType::FileData){
        transferFileData(socket);
    }
}

//发送文件
void FileServer::transferFileData(QTcpSocket *socket)
{
    qint64 payloadSize = 1024*10;    //每一帧发送1024*64个字节，控制每次读取文件的大小，从而传输速度
    double progressByte= 0;//发送进度
    qint64 bytesWritten=0;//已经发送的字节数

    while(bytesWritten != m_sendFileSize){
        double temp = bytesWritten/1.0/m_sendFileSize*100;
        qint64 progress = static_cast<int>(temp);
        if(bytesWritten<m_sendFileSize){
            //从本地文件读取数据块，数据块的大小不能超过qMin(payloadSize,m_sendFileSize)
            QByteArray DataInfoBlock = m_localFile.read(qMin(payloadSize,m_sendFileSize));
            //存储实际写入套接字的数据块大小
            qint64 WriteBlockSize = socket->write(DataInfoBlock,DataInfoBlock.size());

//            QThread::usleep(3); //添加延时，防止服务端发送文件帧过快，否则发送过快，客户端接收不过来，导致丢包

            if(!socket->waitForBytesWritten(3*1000)){
                ui->textBrowser->append(QString("发送超时......"));
                return;
            }
            bytesWritten += WriteBlockSize;
            ui->sendProgressBar->setValue(progress);
        }
        if(bytesWritten == m_sendFileSize){
            ui->textBrowser->append(QString("当前发送进度: %1/%2 -> %3").arg(bytesWritten).arg(m_sendFileSize).arg(progressByte));
            ui->textBrowser->append(QString("=========文件[%1]发送成功========").arg(socket->peerAddress().toString()));
            ui->sendProgressBar->setValue(100);
            m_localFile.close();
            return ;
        }

        if(bytesWritten > m_sendFileSize){
            ui->textBrowser->append("发生未知错误.......");
            return ;
        }
        if(bytesWritten / 1.0 / m_sendFileSize > progressByte){
            ui->textBrowser->append(QString("当前发送进度: %1/%2 -> %3").arg(bytesWritten).arg(m_sendFileSize).arg(progressByte));
            progressByte += 0.1;
        }
    }
}
//发送文件信息
void FileServer::transferFileInfo(QTcpSocket *socket)
{
    //获取指定文件的内容并将其存储
    QByteArray DataInfoBlock = getFileContent(ui->fileEdit->text());
//    QThread::usleep(10);
    m_fileInfoWriteBytes = socket->write(DataInfoBlock) - typeMsgSize;
    qDebug()<< "传输文件信息，大小："<< m_sendFileSize;
    //等待发送完成，才能继续下次发送，否则发送过快，对方无法接收

    if(!socket->waitForBytesWritten(10*1000)){
        ui->textBrowser->append(QString("文件信息发送出来错误,错误为%1").arg(socket->errorString()));
        return ;
    }
    ui->textBrowser->append(QString("文件信息发送完成，现在开始传输文件[%1]----------").arg(socket->localAddress().toString()));
    qDebug()<<"当前文件传输线程id:"<<QThread::currentThreadId();

    m_localFile.setFileName(m_sendFilePath);
    if(!m_localFile.open(QFile::ReadOnly)){
        ui->textBrowser->append(QString("文件[%1]打开失败或文件不存在").arg(m_sendFilePath));
        return ;
    }
}
//返回文件信息
QByteArray FileServer::getFileContent(QString filePath)
{
    if(!QFile::exists(filePath)){
        ui->textBrowser->append("没有要传输的文件"+filePath);
        return "";
    }
    m_sendFilePath = filePath;
    ui->textBrowser->append(QString("正在获取文件[%1]的信息......").arg(filePath));
    QFileInfo info(filePath);

    //获取文件大小
    m_sendFileSize = info.size();
    ui->textBrowser->append(QString("文件[%1]的大小为： %2M").arg(filePath).arg(m_sendFileSize/1024/1024.0));
    //获取文件名称
    QString currentFileName = filePath.right(filePath.size() - filePath.lastIndexOf('/')-1);
    QByteArray DataInfoBlock;

    //通过使用 QDataStream 对象，可以将数据以二进制格式写入到 DataInfoBlock 中
    QDataStream sendOut(&DataInfoBlock,QIODevice::WriteOnly);
    sendOut.setVersion(QDataStream::Qt_5_12);
    int type = MsgType::FileInfo;

    sendOut << int(type) << QString(currentFileName) << qint64(m_sendFileSize);
    ui->textBrowser->append(QString("文件[%1]的信息获取成功").arg(currentFileName));

    QString Msg;
    if(m_sendFileSize > 1024*1024*1){
        Msg = QString("%1M").arg(m_sendFileSize/1024/1024);

    }else{
        Msg = QString("%1KB").arg(m_sendFileSize/1024);
    }
    ui->textBrowser->append(QString("文件名：[%1],文件大小：[%2]").arg(currentFileName).arg(Msg));
    return DataInfoBlock;
}
//检查文件
bool FileServer::checkFile(QString filePath)
{
    QFile file(filePath);
    if(!file.exists(filePath)){
        ui->textBrowser->append(QString("上传文件不存在"));
        return false;
    }

    if(filePath.size()==0){
        ui->textBrowser->append(QString("文件为空！"));
        return false;
    }
    return true;
}

