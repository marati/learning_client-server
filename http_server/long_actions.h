#pragma once

#include <QObject>
#include <QAbstractSocket>

class QTcpSocket;
class QFile;
class Logger;

class LongAction: public QObject
{
    Q_OBJECT

public:
    LongAction(int socketDescriptor, const QString &path,
               bool sending, QObject *parent = 0);
    ~LongAction();

signals:
    void finished(const QString &fileName = QString());

protected slots:
    void socketError(QAbstractSocket::SocketError);
    virtual void reading() = 0;

protected:
    //params - int and string values
    bool sendCommand(int code, const QVariantList &params);

protected:
    QTcpSocket * _commandSocket;
    QFile * _receiveFile;
    Logger * _logger;

    int _partSize;
    int _fileSize;

private:
    void init(const QString &path, bool sending);
};


class SendFileAction: public LongAction
{
    Q_OBJECT

public:
    SendFileAction(int socketDescriptor, const QString &path, bool sending, QObject *parent = 0);

protected slots:
    void reading() {}

private:
    void sendFile();
};


class SaveFileAction: public LongAction
{
    Q_OBJECT

public:
    SaveFileAction(int socketDescriptor, const QString &path, bool sending, QObject *parent = 0);

protected slots:
    void reading();

private:
    void saveFile();

private:
    int _nextBlockSize;
    int _receivingBytes;
};
