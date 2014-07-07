#pragma once

#include <QObject>
#include <QtNetwork>

class Logger;

class WorkerForServer: public QObject
{
    Q_OBJECT

public:
    WorkerForServer(int sd, QObject *parent = 0);
    ~WorkerForServer();

signals:
    void finished();

private slots:
    void endFileSending(const QString &filePath);
    void endFileSaving(const QString &filePath);
    void readRequest();

    void clientDisconnected();
    void socketError(QAbstractSocket::SocketError);

private:
    bool sendCommand(int responseCode, const QString &info);

private:
    Logger * _logger;
    QTcpSocket * _commandSocket;
    int _socketDescriptor;

    /*static*/ QString _currentCommandPath;
    QDir _currentDir;

    enum ChangeStructure
    {
        ChangeCurrentDir = 1,
        ChangeParentDir = 2
    };

    /*static*/ int _colStructureChanges;

};


class TcpServer: public QTcpServer
{
    Q_OBJECT

public:
    TcpServer(QObject *parent = 0);

protected:
    virtual void incomingConnection(int handle);
};
