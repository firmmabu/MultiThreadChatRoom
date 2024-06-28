#include "Thread.h"


const char* uploadFileSignal = "[uploadstart]";
const char* uploadFileFinishSignal = "[uploadfinis]";
const char* getFileListSignal = "[requestfile]";
const char* downloadFileSignal = "[downloadfil]";

ReadThread::ReadThread(QTcpSocket* sock): tcpSocket(sock) {
    connect(this->tcpSocket, &QTcpSocket::readyRead, this, [ = ]() {
        QByteArray msg = this->tcpSocket->readAll();
        emit ReadThread::signalPrintLog("read thread: readMsg[" + QString::fromLocal8Bit(msg) + "]");
        emit ReadThread::readMsg(msg);
    });
}

void ReadThread::run() {
    emit ReadThread::signalPrintLog("ReadThread begin");
    while (!stop) {}
    quit();
}

void ReadThread::closeThread() {
    this->stop = true;
}




WriteThread::WriteThread(QTcpSocket* sock): tcpSocket(sock) {

}

void WriteThread::run() {
    emit WriteThread::signalPrintLog("WriteThread begin");
    while (!stop) {}
    quit();
}

void WriteThread::sendMsg(QByteArray msg) {
    emit WriteThread::signalPrintLog( "write thread: sendMsg[" + QString::fromLocal8Bit(msg) + "]");
    int writeSize = this->tcpSocket->write(msg);
    if (writeSize == -1) emit WriteThread::signalPrintLog( "发送失败");
}

void WriteThread::closeThread() {
    this->stop = true;
}



FileThread::FileThread(QTcpSocket* sock): tcpSocket(sock) {

}

void FileThread::run() {
    emit FileThread::signalPrintLog("FileThread begin");
    while (!stop) {}
    quit();
}

void FileThread::closeThread() {
    this->stop = true;
}

void FileThread::slotSendFile(QString filePath) {
    if (!QFile::exists(filePath)) {
        emit FileThread::signalPrintLog("文件不存在");
        //QMessageBox::warning(this->mainwindow,"文件上传错误","文件不存在");
    } else {
        emit FileThread::signalPrintLog("upload file: " + filePath);
        QStringList tempList = filePath.split('/');
        QString fileName = *tempList.rbegin();

        std::string fileNameStdStr = fileName.toStdString();
        const char* fileNameCStr =  fileNameStdStr.c_str();
        FILE* pFile = nullptr;
        pFile = fopen(filePath.toUtf8().data(), "r");
        if (pFile == nullptr) {
            emit FileThread::signalPrintLog("文件打开失败");
        } else {
            char buffer[512] = "";
            // 发送上传文件的信号给服务器
            memcpy(buffer, ::uploadFileSignal, SIGNAL_SIZE);
            memcpy(buffer + SIGNAL_SIZE, fileNameCStr, strlen(fileNameCStr));
            this->tcpSocket->write(buffer, strlen(buffer));
            this->tcpSocket->waitForBytesWritten(100);

            // 循环发送文件给服务器
            while (!feof(pFile)) {
                QTest::qSleep(100);
                memset(buffer, 0, sizeof(buffer));
                size_t len = fread(buffer, sizeof(char), sizeof(buffer), pFile);
                Q_UNUSED(len)
                this->tcpSocket->write(buffer, strlen(buffer));
                this->tcpSocket->waitForBytesWritten(100);
            }

            // 发送上传完成的信号给服务器
            memset(buffer, 0, sizeof(buffer));
            memcpy(buffer, ::uploadFileFinishSignal, SIGNAL_SIZE);
            memcpy(buffer + SIGNAL_SIZE, fileNameCStr, strlen(fileNameCStr));
            this->tcpSocket->write(buffer, strlen(buffer));
            this->tcpSocket->waitForBytesWritten(100);
            emit FileThread::signalPrintLog("upload file complete: " + filePath);
            fclose(pFile);
        }
    }
}

void FileThread::slotRecvFile(QString fileName) {
    char buffer[512] = "";
    std::string fileNameStdStr = fileName.toStdString();
    const char* fileNameCStr =  fileNameStdStr.c_str();
    emit FileThread::signalPrintLog("选择的文件名: " + fileName);
    emit FileThread::signalPrintLog("download file: " + fileName);
    memcpy(buffer, ::downloadFileSignal, SIGNAL_SIZE);
    memcpy(buffer + SIGNAL_SIZE, fileNameCStr, strlen(fileNameCStr));
    this->tcpSocket->write(buffer, strlen(buffer));
    QString downloadPath = QCoreApplication::applicationDirPath() + "/downloads/" + fileName;
    FILE* pFile = nullptr;
    pFile = fopen(downloadPath.toUtf8().data(), "a+");

    memset(buffer, 0, sizeof(buffer));
    this->tcpSocket->read(buffer, sizeof(buffer));
    while (strcmp(buffer, ::uploadFileFinishSignal) != 0) {
        fputs(buffer, pFile);
        memset(buffer, 0, sizeof(buffer));
        this->tcpSocket->waitForReadyRead(100);
        this->tcpSocket->read(buffer, sizeof(buffer));
    }
    emit FileThread::signalPrintLog("file has been download to: " + downloadPath);
    fclose(pFile);
    pFile = nullptr;
}

void FileThread::slotGetFileList() {
    this->tcpSocket->write(::getFileListSignal, SIGNAL_SIZE);
    this->tcpSocket->waitForBytesWritten(100);
    char fileNames[1024] = "";
    this->tcpSocket->read(fileNames, sizeof(fileNames));
    QStringList fileList = QString::fromLocal8Bit(fileNames).split(',');
    emit FileThread::signalShowFileList(fileList);
}
