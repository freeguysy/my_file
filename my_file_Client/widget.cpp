#include "widget.h"
#include "ui_widget.h"

#include <QDir>
#include <QMessagebox>
#include <QDesktopServices>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    setWindowTitle("小艾客户端");
    ui->portEdit->setText("8888");
    myfileinfo = new MyFileInfo(this);
    //返回上一级并创建下载目录
    m_downloadPath = QCoreApplication::applicationDirPath() + "/../下载";
    isDownloading = false;
    QDir dir;
    if(!dir.exists(m_downloadPath)){
        dir.mkdir(m_downloadPath);
    }
    connectToServer();
    connectSigSlts();

}

Widget::~Widget()
{
    delete ui;
}

void Widget::downloadFile()
{
    if(m_tcp->state() != QAbstractSocket::ConnectingState){
        if(!connectToServer(m_tcp)){
            return ;
        }
    }
    QByteArray data;
    int type =MsgType::FileInfo;
    QDataStream out(&data,QIODevice::WriteOnly);

    out << type;
    ui->textBrowser->append(QString("发送消息..[%1]").arg(type));
    m_tcp->write(data);
}

void Widget::readServerMsg()
{
    if(isDownloading){
        FileDataDown();
        return ;
    }
    qDebug()<< ".............readServerMsg................";

    QDataStream in(m_tcp);
    in.setVersion(QDataStream::Qt_5_12);
    int type;
    in >> type >> myfileinfo->fileName >> myfileinfo->fileSize;
    if(type == MsgType::FileInfo){
        FileInfoDown();
        isDownloading = true;
    }else{
        qDebug() << "收到其他类型的信息:type-" + type;
    }
}

void Widget::FileInfoDown()
{
    ui->textBrowser->append(QString("收到文件信息:文件名称-- %1，文件大小-- %2 KB").arg(myfileinfo->fileName).arg(myfileinfo->fileSize/1024));
    QString filepath = m_downloadPath + "/" + myfileinfo->fileName;
    myfileinfo->localFile.setFileName(filepath);

    //文件写入
    if(!myfileinfo->localFile.open(QIODevice::WriteOnly)){
        QMessageBox::warning(this,"提示","要写入的文件打开失败");
    }
    QByteArray data;
    int type = MsgType::FileData;
    QDataStream out(&data,QIODevice::WriteOnly);
    out << type;
    m_tcp->write(data);
}

void Widget::FileDataDown()
{
    //获取当前可读取的字节数
    qint64 readBytes = m_tcp->bytesAvailable();
    if(readBytes <0)    return ;

    int progress = 0;
    if(myfileinfo->bytesReceived < myfileinfo->fileSize){
        QByteArray data = m_tcp->read(readBytes);
        myfileinfo->bytesReceived += readBytes;
        ui->textBrowser->append(QString("当前接收进度：%1 KB / %2 KB").arg(myfileinfo->bytesReceived/1024).arg(myfileinfo->fileSize/1024));
        //传输进度
        progress = static_cast<int>(myfileinfo->bytesReceived/1.0/myfileinfo->fileSize*100);
        myfileinfo->progressByte = myfileinfo->bytesReceived;
        myfileinfo->progressStr = QString("%1").arg(progress);
        ui->progressBar->setValue(progress);
        myfileinfo->localFile.write(data);
    }
    if (myfileinfo->bytesReceived == myfileinfo->fileSize){
        ui->textBrowser->append(QString("文件[%1]接收成功....").arg(myfileinfo->fileName));
        progress = 100;
        myfileinfo->localFile.close();

        ui->textBrowser->append(QString("当前接收进度：%1 KB / %2 KB").arg(myfileinfo->bytesReceived/1024).arg(myfileinfo->fileSize/1024));
        myfileinfo->progressByte = myfileinfo->bytesReceived;
        //读取成功，重新初始化
        ui->progressBar->setValue(progress);
        isDownloading = false;
        myfileinfo->initReadData();
    }


    if (myfileinfo->bytesReceived > myfileinfo->fileSize){
        QMessageBox::warning(this,"警告","发生未知错误，接收数据大于文件大小");
    }
}

void Widget::connectSigSlts()
{
    connect(ui->downloadBtn,&QPushButton::clicked,this,&Widget::downloadFile);
    connect(ui->connectBtn,&QPushButton::clicked,[=](){
        connectToServer(m_tcp);
    });
    connect(ui->disconnectBtn,&QPushButton::clicked,[=](){
        m_tcp->disconnectFromHost();
        ui->textBrowser->append("与服务器断开连接......");
    });
    connect(ui->resetProgress,&QPushButton::clicked,[=](){
       ui->progressBar->setValue(0);
    });
    connect(ui->openFloder,&QPushButton::clicked,[=](){
        QString path = QCoreApplication::applicationDirPath() + "/../下载";
        //打开指定的url,QUrl::TolerantMode 可以增加URL的容错性，使得即使URL格式不完全正确，也能够成功解析URL并打开相应的文件或资源。
        QDesktopServices::openUrl(QUrl("file:"+path, QUrl::TolerantMode));
    });
}

void Widget::connectToServer()
{
    m_tcp = new QTcpSocket(this);
    connectToServer(m_tcp);
    connect(m_tcp,&QTcpSocket::readyRead,this,&Widget::readServerMsg);
    connect(m_tcp,&QTcpSocket::disconnected,[=](){
        ui->textBrowser->append(QString("与服务器断开连接,原因--%1").arg(m_tcp->errorString()));
    });

}

bool Widget::connectToServer(QTcpSocket *socket)
{
    socket->connectToHost(ui->ipEdit->text(),ui->portEdit->text().toInt());
    if(!socket->waitForConnected(2*1000)){
        //QMessageBox::warning(this,"警告","连接超时，错误信息为"+socket->errorString());
        return false;
    }
    QMessageBox::information(this,"提示","恭喜您连接成功");
    ui->textBrowser->append("成功连接服务器.........");
    return true;
}

