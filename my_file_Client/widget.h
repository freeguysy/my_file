#ifndef WIDGET_H
#define WIDGET_H
#include "myfileinfo.h"
#include <QWidget>
#include <QTcpSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

enum MsgType{
    FileInfo,
    FileData,
};

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    void downloadFile();
    void readServerMsg();
    void FileInfoDown();
    void FileDataDown();
private:
    void connectSigSlts();
    void connectToServer();
    bool connectToServer(QTcpSocket* m_tcp);
private:
    Ui::Widget *ui;
    QTcpSocket* m_tcp;
    QString m_downloadPath;
    MyFileInfo* myfileinfo;
    bool isDownloading;
};
#endif // WIDGET_H
