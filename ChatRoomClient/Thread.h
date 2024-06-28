#ifndef THREAD_H
#define THREAD_H

#include <QThread>
#include <QTcpSocket>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <stdio.h>
#include <QTest>
#include <stdio.h>
#include <QString>

#define SIGNAL_SIZE 13

class ReadThread : public QThread {
    Q_OBJECT

public:
    ReadThread(QTcpSocket* sock);

protected:
    void run() override;

private:
    QTcpSocket* tcpSocket;
    bool stop = false;

public slots:
    void closeThread();

signals:
    void readMsg(QByteArray msg);
    void signalPrintLog(QString str);
};


class WriteThread : public QThread {
    Q_OBJECT
public:
    WriteThread(QTcpSocket* sock);

protected:
    void run() override;

private:
    QTcpSocket* tcpSocket;
    bool stop = false;

public slots:
    void sendMsg(QByteArray);
    void closeThread();

signals:
    void signalPrintLog(QString str);
};

class FileThread : public QThread {
    Q_OBJECT
public:
    FileThread(QTcpSocket* sock);


protected:
    void run() override;

private:
    QTcpSocket* tcpSocket;
    bool stop = false;

public slots:
    void closeThread();
    void slotSendFile(QString fileName);
    void slotRecvFile(QString fileName);
    void slotGetFileList();

signals:
    void signalPrintLog(QString str);
    void signalShowFileList(QStringList str);
};

#endif // THREAD_H
