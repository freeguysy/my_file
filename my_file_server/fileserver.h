 #ifndef FILESERVER_H
#define FILESERVER_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>

namespace Ui {
class FileServer;
}

//消息类型
enum MsgType{
    FileInfo,   //文件信息，如文件名，文件大小等信息
    FileData,   //文件数据，即文件内容
};

class FileServer : public QMainWindow
{
    Q_OBJECT

public:
    explicit FileServer(QWidget *parent = nullptr);
    ~FileServer();

    void connectSigSlots();
    bool startServer();
public slots:
    void delNewConnect();
    void dealMsg(QTcpSocket* socket);

private:
    void transferFileData(QTcpSocket* socket);
    void transferFileInfo(QTcpSocket* socket);
    QByteArray getFileContent(QString filePath);
    bool checkFile(QString filePath);
private:
    Ui::FileServer *ui;
    QTcpServer* m_tcpServer;

private:

    qint64 typeMsgSize;
    qint64 m_totalBytes;
    //要发送的文件大小
    qint64 m_sendFileSize;
    //实际文件的字节
    qint64 m_fileInfoWriteBytes;
    //文件路径，本地文件
    QString m_sendFilePath;
    QFile m_localFile;
};

#endif // FILESERVER_H
