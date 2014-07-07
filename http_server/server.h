#pragma once

#include <QtNetwork>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>

#include "tcp_server.h"
#include "http_server.h"
#include "logger.h"

class Server: public QObject
{
    Q_OBJECT

public:
    Server(QObject *parent = 0);
    ~Server();

//signals:
//    void beginChangesSending(const QString &);

private slots:
    //ip & port - depreced parameters
    void startStructureTransfer(const QHostAddress &ip = QHostAddress(), int port = 0);
    void directoryChanged(const QString &);
    void fileChanged(const QString &);
    void socketError(QAbstractSocket::SocketError);

private:
    void startScanning();
    void sendRequest(const QByteArray &, const QHostAddress &ip, int port);

private:
    Logger * _logger;
    QString _selectedDirectory;

    QUdpSocket * _udpSocket;
    TcpServer * _tcpServer;
    HttpServer * _httpServer;
    QFileSystemWatcher * _systemWatcher;

};
