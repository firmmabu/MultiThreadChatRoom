#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);

    // 无边框窗口
    setWindowFlag(Qt::FramelessWindowHint);
    // 固定宽和高
    setFixedSize(this->width(), this->height());

    // 初始化控件
    this->buttonExit = ui->buttonExit;
    this->buttonSend = ui->buttonSend;
    this->buttonConnect = ui->buttonConnect;
    this->buttonUploadFile = ui->buttonUpload;
    this->buttonDownloadFile=ui->buttonDownload;
    this->textEdit = ui->textEdit;
    this->textEdit->installEventFilter(this);
    this->log = ui->editLog;
    this->listFiles = ui->listFiles;

    this->log->setReadOnly(true);
    ui->editAddress->setText("192.168.1.52");
    ui->editMsgPort->setText("8888");
    ui->editFilePort->setText("8887");
    this->listFiles->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(this->buttonExit, &QPushButton::clicked, this, &MainWindow::slotExitApp);
    connect(this->buttonSend, &QPushButton::clicked, this, &MainWindow::sendMsg);
    connect(this->buttonUploadFile, &QPushButton::clicked, this, &MainWindow::slotUploadFile);
    connect(this->buttonDownloadFile,&QPushButton::clicked,this,&MainWindow::slotDownloadFile);

    // 连接到服务器
    this->msgSock = new QTcpSocket(this);
    this->fileSock = new QTcpSocket(this);
    connect(this->buttonConnect, &QPushButton::clicked, this, &MainWindow::slotConnectServer);

    // 新开三个线程，一个用于发送数据到服务器，一个用于接收服务器发过来的数据，一个用于发送和接收文件
    this->readThread = new ReadThread(this->msgSock);
    this->writeThread = new WriteThread(this->msgSock);
    this->fileThread = new FileThread(this->fileSock);
    connect(this, &MainWindow::signalStopThread, this->readThread, &ReadThread::closeThread);
    connect(this, &MainWindow::signalStopThread, this->writeThread, &WriteThread::closeThread);
    connect(this, &MainWindow::signalStopThread, this->fileThread, &FileThread::closeThread);
    connect(this->readThread, &ReadThread::signalPrintLog, this, &MainWindow::slotPrintLog);
    connect(this->writeThread, &WriteThread::signalPrintLog, this, &MainWindow::slotPrintLog);
    connect(this->fileThread, &FileThread::signalPrintLog, this, &MainWindow::slotPrintLog);

    connect(this, &MainWindow::signalSend, this->writeThread, &WriteThread::sendMsg);
    connect(this, &MainWindow::signalUploadFile, this->fileThread, &FileThread::slotSendFile);
    connect(this,&MainWindow::signalDownloadFile,this->fileThread,&FileThread::slotRecvFile);
    connect(this->readThread, &ReadThread::readMsg, this, &MainWindow::showRecivedMsg);
    connect(this->fileThread, &FileThread::signalShowFileList, this, &MainWindow::showFileList);

    // 十秒钟更新一次文件列表
    this->timer=new QTimer(this->fileThread);
    connect(this->timer,SIGNAL(timeout()),this->fileThread,SLOT(slotGetFileList()));
}

MainWindow::~MainWindow() {
    delete ui;
    emit MainWindow::signalStopThread();
    msgSock->disconnectFromHost();
}

void MainWindow::showSendMsg(QByteArray msg) {
}

typedef QPoint vector;

void MainWindow::mouseMoveEvent(QMouseEvent* event) {
    vector c = event->globalPos() - this->pos();
    vector b = c - this->position;
    this->move(b + this->pos());
}

void MainWindow::mousePressEvent(QMouseEvent* event) {
    vector a = event->globalPos() - this->pos();
    this->position = a;
}

bool MainWindow::eventFilter(QObject* target, QEvent* event) {
    // 按下回车键发送消息
    if (target == ui->textEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return) {
            this->buttonSend->click();
            return true;
        }
    }

    return QWidget::eventFilter(target, event);
}

void MainWindow::slotExitApp() {
    qApp->exit(0);
}

void MainWindow::slotConnectServer() {
    QString serverAddr = ui->editAddress->text();
    QString msgPortStr = ui->editMsgPort->text();
    QString filePortStr = ui->editFilePort->text();

    if (serverAddr == "" || msgPortStr == "" || filePortStr == "") {
        QMessageBox::warning(this, "连接错误", "ip和端口不能为空", QMessageBox::Ok);
    } else {
        quint16 msgPort = msgPortStr.toUInt();
        quint16 filePort = filePortStr.toUInt();
        this->msgSock->connectToHost(serverAddr, msgPort);
        this->fileSock->connectToHost(serverAddr, filePort);
        bool connectRet1 = false,connectRet2=false;
        connectRet1 = this->msgSock->waitForConnected(5000);
        connectRet2 = this->fileSock->waitForConnected(5000);
        if(connectRet1&&connectRet2){
            ui->labelChatEnable->setText("可用");
            ui->labelFileEnable->setText("可用");
            this->readThread->start();
            this->writeThread->start();
            this->fileThread->start();
            this->timer->start(10000);
        }else{
            QMessageBox::warning(this,"连接错误","连接服务器超时");
        }
    }
}

void MainWindow::slotUploadFile() {
    if (this->fileSock->state() != QTcpSocket::ConnectedState) {
        QMessageBox::warning(this, "文件传输错误", "发送文件功能不可用!");
    } else {
        QString defaultPath = QCoreApplication::applicationDirPath();
        QString fileName = QFileDialog::getOpenFileName(this, "选择文件", defaultPath, "*.*");
        emit MainWindow::signalUploadFile(fileName);
    }
}

void MainWindow::slotDownloadFile(){
    QListWidgetItem* selectedItem=this->listFiles->currentItem();
    if(selectedItem==nullptr){
        QMessageBox::warning(this,"文件下载错误","未选择要下载的文件");
    }else{
        emit MainWindow::signalDownloadFile(selectedItem->text());
    }
}

void MainWindow::slotPrintLog(QString str) {
    this->log->appendPlainText(str + '\n');
}

void MainWindow::showFileList(QStringList fileList) {
    this->listFiles->clear();
    int c = fileList.size();
    for (int i = 0; i < c - 1; ++i) {
        this->listFiles->addItem(fileList[i]);
    }
}

int MainWindow::sendMsg() {
    QString str = this->textEdit->toPlainText();
    if (str == "") {
        QMessageBox::warning(this, "发送错误", "输入为空!");
    } else {
        this->textEdit->clear();
        this->textEdit->setFocus();
        ui->listMsg->addItem(str);
        QByteArray msg = str.toUtf8();
        emit MainWindow::signalSend(msg);
    }
    return 0;
}

void MainWindow::showRecivedMsg(QByteArray msg) {
    ui->listMsg->addItem(QString::fromLocal8Bit(msg));
}
