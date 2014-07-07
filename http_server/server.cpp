#include "server.h"
#include "dir_scanner.h"

#include <QApplication>
#include <QThread>
#include <QDebug>

Server::Server(QObject *parent):
    QObject(parent)/*,
    _structureCreate(false)*/
{
    _logger = Logger::instance();

    _selectedDirectory = QString::fromUtf8("/home/marat/study/test_dir");

//    int randPort = 0;
//    while (randPort < 1025)
//    {
//        qsrand(QDateTime::currentDateTime().toTime_t() + int(this));
//        randPort = qrand()%10000;
//    }

    _udpSocket = new QUdpSocket(this);
    connect(_udpSocket,
            SIGNAL(error(QAbstractSocket::SocketError)),
            SLOT(socketError(QAbstractSocket::SocketError)));

    _systemWatcher = new QFileSystemWatcher(this);
    connect(_systemWatcher, SIGNAL( directoryChanged(QString) ),    SLOT( directoryChanged(QString) ));
    connect(_systemWatcher, SIGNAL( fileChanged(QString) ),         SLOT( fileChanged(QString) ));

    _tcpServer = new TcpServer(this);
    if (!_tcpServer->listen(QHostAddress::Any, 15449))
        _logger->addEntry("Server failed to start", true);

    //_tcpServer->connect(this, SIGNAL( beginChangesSending(QString) ), SIGNAL( changesSending(QString) ));

    _httpServer = new HttpServer(this);
    if (!_httpServer->listen(QHostAddress::Any, 8080))
        _logger->addEntry("Http Server failed to start", true);

    startScanning();
}

Server::~Server()
{
    _logger->freeInstance();
}

void Server::startScanning()
{
    DirScanner *scanner = new DirScanner("structure.xml", _selectedDirectory, _systemWatcher);

    scanner->connect(scanner, SIGNAL( scanned() ), SLOT( quit() ));
    connect(scanner, SIGNAL( scanned() ), SLOT( startStructureTransfer() ));

    scanner->start();
}

void Server::sendRequest(const QByteArray &datagram, const QHostAddress &ip, int port)
{
    if (_udpSocket->writeDatagram(datagram, ip, port) == -1)
    {
        _logger->addEntry("fail write datagram" + _udpSocket->errorString(), true);
    }

    _logger->addEntry("Sent datagram (size +" + QString::number(datagram.size()) + ")");
}

void Server::startStructureTransfer(const QHostAddress &ip, int port)
{
    QByteArray datagram;
    QDataStream out(&datagram, QIODevice::WriteOnly);
    out << QString("START_STRUCTURE_TRANSFER") << (int)15449;

    if (ip.isNull())
    {
        sendRequest(datagram, QHostAddress::Broadcast, 15444);
    }
    else
    {
        sendRequest(datagram, ip, port);
    }
}

void Server::directoryChanged(const QString &path)
{
    qDebug() << "dic ch" << path;
    startScanning();
}

void Server::fileChanged(const QString &fileName)
{
    qDebug() << "file Ch" << fileName;
}

void Server::socketError(QAbstractSocket::SocketError err)
{
    QString strError =
        "Error: " + (err == QAbstractSocket::HostNotFoundError ?
                     "The host was not found." :
                     err == QAbstractSocket::RemoteHostClosedError ?
                     "The remote host is closed." :
                     err == QAbstractSocket::ConnectionRefusedError ?
                     "The connection was refused." :
                     QString(_udpSocket->errorString())
                    );
    _logger->addEntry(strError, true);
}
