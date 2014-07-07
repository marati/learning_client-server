#include "long_actions.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QDir>
#include <QDateTime>

LongAction::LongAction(const QString &serverIp, int port,
                       const QString &command, QObject *parent):
    QObject(parent),
    _nextBlockSize(0),
    _responseCode(0)
{
    QTcpSocket *tcpSocket = new QTcpSocket(this);
    //connect(tcpSocket, SIGNAL( connected() ), SLOT( connected() ));
    connect(tcpSocket, SIGNAL( disconnected() ), tcpSocket, SLOT( deleteLater() ));
    connect(tcpSocket, SIGNAL( readyRead() ), SLOT( readResponse() ));
    connect(tcpSocket,
            SIGNAL(error(QAbstractSocket::SocketError)),
            SLOT(socketError(QAbstractSocket::SocketError)));

    tcpSocket->connectToHost(serverIp, port);
    tcpSocket->waitForConnected();

    _receiveFile = new QFile(this);

    QTextStream out(tcpSocket);
    out << command;
    out.flush();

    emit addToLog(command);
}

LongAction::~LongAction()
{
    qDebug() << "~LongAction()";
}

void LongAction::socketError(QAbstractSocket::SocketError err)
{
    QTcpSocket *clientSocket = (QTcpSocket*)sender();

    QString strError =
        "Error: " + (err == QAbstractSocket::HostNotFoundError ?
                     "The host was not found." :
                     err == QAbstractSocket::RemoteHostClosedError ?
                     "The remote host is closed." :
                     err == QAbstractSocket::ConnectionRefusedError ?
                     "The connection was refused." :
                     QString(clientSocket->errorString())
                    );
    emit addToLog(strError, true);
}



DownloadAction::DownloadAction(const QString &serverIp, int port,
                               const QString &command, QObject *parent):
    LongAction(serverIp, port, command, parent),
    _fileSize(0),
    _receivingBytes(0)
{
}

//void DownloadAction::connected()
//{
//    emit addToLog("Client connected to server in order to download file.");
//}

void DownloadAction::readResponse()
{
    QTcpSocket *clientSocket = (QTcpSocket*)sender();
    QDataStream in(clientSocket);

    for (;;)
    {
        if (!_nextBlockSize)
        {
            if (clientSocket->bytesAvailable() < sizeof(quint32))
                break;

            in >> _nextBlockSize;
        }

        if (clientSocket->bytesAvailable() < _nextBlockSize)
            break;

        if (!_responseCode)
        {
            in >> _responseCode;

            QString info;
            QString fileName;
            int range = 0;

            if (_responseCode == 150)
            {
                in >> fileName >> _fileSize >> range;

                if (!QFile::exists("download_files"))
                {
                    QDir dir(QDir::currentPath());
                    dir.mkdir("download_files");
                }

                if (fileName.contains("structure"))
                {
                    int randNum = 0;
                    while (randNum < 1025)
                    {
                        qsrand(QDateTime::currentDateTime().toTime_t() + int(this));
                        randNum = qrand()%10000;
                    }

                    int insertPos = fileName.indexOf(".xml");
                    fileName = fileName.insert(insertPos, "_" + QString::number(randNum));
                    _structureFilePath = QDir::currentPath() + "/download_files/" + fileName;
                }

                QString filePath = QDir::currentPath() + "/download_files/" + fileName;
                _receiveFile->setFileName(filePath);

                if (!_receiveFile->open(QFile::WriteOnly))
                {
                    emit addToLog("Error open file " + _receiveFile->fileName() + " : " + _receiveFile->errorString(), true);
                    return;
                }

                _nextBlockSize = 0;
            }
            else if (_responseCode == 450)
            {
                in >> info;
            }

            QString response = "Response: " + QString::number(_responseCode);
            if (!info.isEmpty())
                response += ": " + info;

            emit addToLog(response);
            continue;
        }

        char data[_nextBlockSize];
        int outData = in.readRawData(data, sizeof(char) * _nextBlockSize);
        _receiveFile->write(data, sizeof(char) * outData);
        _receivingBytes += outData;

        _nextBlockSize = 0;

        if (_receivingBytes == _fileSize)
        {
            emit addToLog("File " + _receiveFile->fileName() + " received");
            _receiveFile->close();

            if (!_structureFilePath.isEmpty())
                emit finished(_structureFilePath);
            else
                emit finished();

            return;
        }
    }
}


UploadAction::UploadAction(const QString &serverIp, int port,
                           const QString &command, const QString &filePath, QObject *parent):
    LongAction(serverIp, port, command, parent)
{
    _receiveFile->setFileName(filePath);
    if (!_receiveFile->open(QFile::ReadOnly))
        emit addToLog("Not open file \""+filePath+"\" for readnig.", true);
}

void UploadAction::readResponse()
{
    QTcpSocket *clientSocket = (QTcpSocket*)sender();
    QDataStream in(clientSocket);

    for (;;)
    {
        if (!_nextBlockSize)
        {
            if (clientSocket->bytesAvailable() < sizeof(quint32))
                break;

            in >> _nextBlockSize;
        }

        if (clientSocket->bytesAvailable() < _nextBlockSize)
            break;

        if (!_responseCode)
        {
            QString info;
            in >> _responseCode >> info;

            QString response = "Response: " + QString::number(_responseCode) + ": " + info;
            emit addToLog(response);

            if (_responseCode == 250)
            {
                QTextStream out(clientSocket);
                QFileInfo fi(_receiveFile->fileName());
                out << "STOR " + fi.fileName();
                out.flush();

                _responseCode = 0;
                _nextBlockSize = 0;
            }
            else if (_responseCode == 150)
            {
                uploadFile(clientSocket);
            }
        }

    }
}

void UploadAction::uploadFile(QTcpSocket *clientSocket)
{
    int fileSize = _receiveFile->size();

    //sending  file size
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << quint32(0) << fileSize;
    out.device()->seek(0);
    out << quint32(data.size() - sizeof(quint32));

    if (clientSocket->write(data) == -1)
    {
        emit addToLog("Error write response " + QString::number(150) + "on tcp socket", true);
        return;
    }

    clientSocket->waitForBytesWritten();
    emit addToLog("Response to request 150");
    //

    int pos = 0;
    int partSize = 1024;
    //int range = fileSize < partSize ? 1 : fileSize/partSize;

    while (pos < fileSize)
    {
        int blockSize = qMin(partSize, fileSize - pos);

        char data[blockSize];
        QDataStream fromFile(_receiveFile);
        int outData = fromFile.readRawData(data, sizeof(char) * (blockSize));

        QByteArray dataBytes;
        QDataStream out(&dataBytes, QIODevice::WriteOnly);
        out << quint32(0);
        out.writeRawData(data, sizeof(char)*outData);
        out.device()->seek(0);
        out << quint32(dataBytes.size() - sizeof(quint32));

        if (clientSocket->write(dataBytes) == -1)
        {
            emit addToLog("Error write part of file " + _receiveFile->fileName(), true);
            return;
        }

        clientSocket->waitForBytesWritten();

        pos += blockSize;
        emit addToLog("Write part of file ("+QString::number(blockSize)+"). Transferred "+QString::number(pos)+" bytes.");
    }

    _receiveFile->close();
    emit finished();
}
