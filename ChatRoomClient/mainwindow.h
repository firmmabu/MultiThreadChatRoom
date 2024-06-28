#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMouseEvent>
#include <QWidget>
#include <QPoint>
#include <QPushButton>
#include <QDebug>
#include <QTextEdit>
#include <QString>
#include <QMessageBox>
#include <QTcpSocket>
#include <QTest>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QTimer>
#include "Thread.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QWidget {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void showSendMsg(QByteArray msg);


protected:
    void mouseMoveEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    bool eventFilter(QObject* target, QEvent* event);

private:
    Ui::MainWindow *ui;

    QPoint position;

    QPushButton* buttonExit;
    QPushButton* buttonSend;
    QPushButton* buttonConnect;
    QPushButton* buttonUploadFile;
    QPushButton* buttonDownloadFile;
    QTextEdit* textEdit;
    QPlainTextEdit* log;
    QListWidget* listFiles;

    QTcpSocket* msgSock;
    QTcpSocket* fileSock;
    ReadThread* readThread;
    WriteThread* writeThread;
    FileThread* fileThread;

    QTimer* timer;

public slots:
    void slotExitApp();
    void slotConnectServer();
    void slotUploadFile();
    void slotDownloadFile();
    int sendMsg();
    void showRecivedMsg(QByteArray msg); // 显示 读线程收到的消息
    void slotPrintLog(QString str);
    void showFileList(QStringList fileList);

signals:
    void signalSend(QByteArray msg);
    void signalStopThread();
    void signalUploadFile(QString fileName);
    void signalDownloadFile(QString fileName);
};
#endif // MAINWINDOW_H
