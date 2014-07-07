#include "tcp_server.h"
#include "long_actions.h"
#include "logger.h"

#include <QDebug>

//QString WorkerForServer::_currentCommandPath = "";
//int WorkerForServer::_colStructureChanges = 0;

WorkerForServer::WorkerForServer(int sd, QObject *parent):
    QObject(parent),
    _socketDescriptor(sd),
    _colStructureChanges(0)
{
    _logger = Logger::instance();

    _commandSocket = new QTcpSocket(this);
    if (!_commandSocket->setSocketDescriptor(_socketDescriptor))
        _logger->addEntry(_commandSocket->errorString(), true);

    connect(_commandSocket, SIGNAL( readyRead() ),     SLOT( readRequest() ));
    connect(_commandSocket, SIGNAL( disconnected() ),  SLOT( clientDisconnected() ));
    connect(_commandSocket,
            SIGNAL(error(QAbstractSocket::SocketError)),
            SLOT(socketError(QAbstractSocket::SocketError)));

    _logger->addEntry("Server: connected! IP: " + _commandSocket->peerAddress().toString()
                      +", port: " + QString::number(_commandSocket->peerPort()));
}

WorkerForServer::~WorkerForServer()
{
    _logger->freeInstance();
}

bool WorkerForServer::sendCommand(int responseCode, const QString &info)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << quint32(0) << responseCode << info;

    out.device()->seek(0);
    out << quint32(data.size() - sizeof(quint32));

    if (_commandSocket->write(data) == -1)
    {
        _logger->addEntry("Error write response " + QString::number(responseCode) + " , info (" + info + ") on tcp socket", true);
        return false;
    }

    _commandSocket->waitForBytesWritten();

    _logger->addEntry("Send response: " + QString::number(responseCode));
    return true;
}

void WorkerForServer::endFileSending(const QString &filePath)
{
    _logger->addEntry("End sending file " + filePath);
    _commandSocket->deleteLater();
    emit finished();
}

void WorkerForServer::endFileSaving(const QString &filePath)
{
    _logger->addEntry("End saving file " + filePath);
    _commandSocket->deleteLater();
    emit finished();
}

void WorkerForServer::readRequest()
{
    QTextStream in(_commandSocket);
    QString request(in.readLine());
    _logger->addEntry("Client: " + request);

    QStringList requestTokens = request.split(" ");
    QString command = requestTokens[0];
    QString path = request.remove(0, command.size()+1);

    if (command == "GIVE_STRUCTURE")
    {
        if (!_currentCommandPath.isEmpty())
        {
            ++_colStructureChanges;

            if (_colStructureChanges != ChangeParentDir)
                return;
        }

        SendFileAction *sendFile = new SendFileAction(_commandSocket->socketDescriptor(), QDir::currentPath() + "/structure.xml",
                                                      true, this);
        connect(sendFile, SIGNAL( finished(QString) ), SLOT( endFileSending(QString) ));
    }
    else if (command == "RETR")
    {
        SendFileAction *sendFile = new SendFileAction(_commandSocket->socketDescriptor(), path, true, this);
        connect(sendFile, SIGNAL( finished(QString) ), SLOT( endFileSending(QString) ));
    }
    else if (command == "MKD")
    {
        QDir dir;
        if (dir.mkdir(path))
            sendCommand(257, path + " - Directory was created successfully");
        else
            sendCommand(451, path + "Directory is not created");
    }
    else if (command == "DELE")
    {
        QDir dir;
        if (dir.remove(path))
            sendCommand(250, "Successfully completed");
        else
            sendCommand(450, "Failed");
    }
    else if (command == "RMD")
    {
        QDir dir;
        if (dir.rmpath(path))
        {
            _currentCommandPath = path;
            sendCommand(250, "Successfully completed");
        }
        else
            sendCommand(451, "Failed");
    }
    else if (command == "RNFR")
    {
        QDir dir;
        if (dir.exists(path))
        {
            _currentCommandPath = path;
            sendCommand(350, "The file or directory exists, ready for destination name");
        }
        else
        {
            sendCommand(451, "The file or directory does not exist");
        }
    }
    else if (command == "RNTO")
    {
        QDir dir;
        if (dir.rename(_currentCommandPath, path))
            sendCommand(250, "Name changed");
        else
            sendCommand(451, "The name is not changed");

        _currentCommandPath.clear();
    }
    else if (command == "CWD")
    {
        _currentDir.setPath(path);

        if (!_currentDir.absolutePath().isEmpty())
            sendCommand(250, "CWD successfully");
        else
            sendCommand(450, "Requested file action not taken");
    }
    else if (command == "STOR")
    {
        //_currentCommandPath = path;

        _commandSocket->disconnect(SIGNAL(readyRead()));
        SaveFileAction *saveAction = new SaveFileAction(_commandSocket->socketDescriptor(), _currentDir.absolutePath() + "/" + path, false, this);
        connect(saveAction, SIGNAL( finished(QString) ), SLOT( endFileSaving(QString) ));
    }
    else
    {
        do {
            qDebug() << "cli : " << path;
            path = in.readLine();
        } while (!in.atEnd());
    }

}

void WorkerForServer::clientDisconnected()
{
    QTcpSocket *clientSocket = (QTcpSocket*)sender();
    _logger->addEntry("Client (" + clientSocket->peerAddress().toString() + ") disconnected.");
    clientSocket->deleteLater();
    emit finished();
}

void WorkerForServer::socketError(QAbstractSocket::SocketError err)
{
    QTcpSocket *_clientSocket = (QTcpSocket*)sender();

    QString strError =
        "Error: " + (err == QAbstractSocket::HostNotFoundError ?
                     "The host was not found." :
                     err == QAbstractSocket::RemoteHostClosedError ?
                     "The remote host is closed." :
                     err == QAbstractSocket::ConnectionRefusedError ?
                     "The connection was refused." :
                                 QString(_clientSocket->errorString())
                    );
    _logger->addEntry(strError, true);
}



TcpServer::TcpServer(QObject *parent):
    QTcpServer(parent)
{
}

void TcpServer::incomingConnection(int handle)
{
    WorkerForServer *worker = new WorkerForServer(handle);
    QThread *serverThread = new QThread(this);
    worker->moveToThread(serverThread);

    serverThread->connect(worker, SIGNAL( finished() ), SLOT( quit() ));
    serverThread->start();
}
