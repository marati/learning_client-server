#include "long_actions.h"
#include "logger.h"

#include <QTcpSocket>
#include <QHostAddress>
#include <QDir>
#include <QDateTime>

LongAction::LongAction(int socketDescriptor, const QString &path,
                       bool sending, QObject *parent):
    QObject(parent),
    _partSize(1024),
    _fileSize(0)
{
    _logger = Logger::instance();

    _commandSocket = new QTcpSocket(this);
    if (!_commandSocket->setSocketDescriptor(socketDescriptor))
        _logger->addEntry(_commandSocket->errorString(), true);

    connect(_commandSocket,
            SIGNAL(error(QAbstractSocket::SocketError)),
            SLOT(socketError(QAbstractSocket::SocketError)));

    connect(_commandSocket, SIGNAL( readyRead() ), SLOT( reading() ));

    if (sending)
        init(path, sending);
    else
        sendCommand(150, QVariantList() << "gg");
}

LongAction::~LongAction()
{
    _logger->freeInstance();
}

void LongAction::init(const QString &path, bool sending)
{
    QFile::OpenMode flags;
    QVariantList params;

    if (sending)
    {
        flags |= QFile::ReadOnly;

        QFileInfo fileInfo(path);
        QString fileName(fileInfo.fileName());
        params << fileName;
    }
    else
    {
        flags |= QFile::WriteOnly;
        params << QString::fromUtf8("Сan accept the file");
    }

    _receiveFile = new QFile(path, this);
    if (!_receiveFile->open(flags))
    {
        _logger->addEntry("Not open file \""+path+"\" for readnig.", true);
        sendCommand(450, QVariantList() << "File " + path + "can't open and cab't be to accept");
        return;
    }

    if (sending)
    {
        _fileSize = _receiveFile->size();
        int range = _fileSize < _partSize ? 1 : _fileSize/_partSize;

        params << _fileSize << range;
    }

    sendCommand(150, params);
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
    _logger->addEntry(strError, true);
}

bool LongAction::sendCommand(int code, const QVariantList &params)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << quint32(0) << code;

    foreach (QVariant param, params)
    {
        if (param.type() == QVariant::Int)
            out << param.toInt();
        else if (param.type() == QVariant::String)
            out << param.toString();
    }

    out.device()->seek(0);
    out << quint32(data.size() - sizeof(quint32));

    if (_commandSocket->write(data) == -1)
    {
        _logger->addEntry("Error write response " + QString::number(code) + " on tcp socket", true);
        return false;
    }

    _commandSocket->waitForBytesWritten();

    _logger->addEntry("Send response: " + QString::number(code));
    return true;
}


SendFileAction::SendFileAction(int socketDescriptor, const QString &path, bool sending, QObject *parent):
    LongAction(socketDescriptor, path, sending, parent)
{
    sendFile();
}

void SendFileAction::sendFile()
{
    int pos = 0;
    QFileInfo fi(*_receiveFile);

    while (pos < _fileSize)
    {
        int blockSize = qMin(_partSize, _fileSize - pos);

        char data[blockSize];
        QDataStream fromFile(_receiveFile);

        int outData = fromFile.readRawData(data, sizeof(char) * (blockSize));
        //QByteArray dataBytes(data, sizeof(char)*outData);

        QByteArray dataBytes;
        QDataStream out(&dataBytes, QIODevice::WriteOnly);
        out << quint32(0);
        out.writeRawData(data, sizeof(char)*outData);
        out.device()->seek(0);
        out << quint32(dataBytes.size() - sizeof(quint32));

        if (_commandSocket->write(dataBytes) == -1)
        {
            _logger->addEntry("Error write part of file " + fi.fileName(), true);
            return;
        }

        _commandSocket->waitForBytesWritten();

        pos += blockSize;
    }

    _receiveFile->close();
    emit finished(_receiveFile->fileName());
}


SaveFileAction::SaveFileAction(int socketDescriptor, const QString &path, bool sending, QObject *parent):
    LongAction(socketDescriptor, path, sending, parent),
    _nextBlockSize(0),
    _receivingBytes(0)
{

}

void SaveFileAction::reading()
{
    qDebug() << "savingFile";
    QDataStream in(_commandSocket);

    for (;;)
    {
        if (!_nextBlockSize)
        {
            if (_commandSocket->bytesAvailable() < sizeof(quint32))
                break;

            in >> _nextBlockSize;
        }

        if (_commandSocket->bytesAvailable() < _nextBlockSize)
            break;

        if (!_fileSize)
        {
            in >> _fileSize;
            qDebug() << "save file " << _fileSize;
            continue;
        }

        char data[_nextBlockSize];
        int outData = in.readRawData(data, sizeof(char) * _nextBlockSize);
        _receiveFile->write(data, sizeof(char) * outData);
        _receivingBytes += outData;
        qDebug() << "received " << _receivingBytes;

        _nextBlockSize = 0;

        if (_receivingBytes == _fileSize)
        {
            _logger->addEntry("File " + _receiveFile->fileName() + " received");
            sendCommand(226, QVariantList() << "Closing data connection");
            _receiveFile->close();
            emit finished(_receiveFile->fileName());
        }
    }
}
