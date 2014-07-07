#pragma once

#include <QObject>
#include <QAbstractSocket>

class QTcpSocket;
class QFile;

class LongAction: public QObject
{
    Q_OBJECT

public:
    LongAction(const QString &serverIp, int port,
               const QString &command, QObject *parent = 0);
    ~LongAction();

signals:
    void addToLog(const QString &, bool isError = false);
    void finished(const QString &fileName = QString());

protected slots:
    virtual void readResponse() = 0;
    void socketError(QAbstractSocket::SocketError);

protected:
    QFile * _receiveFile;
    int _nextBlockSize;
    int _responseCode;
};



class DownloadAction: public LongAction
{
    Q_OBJECT

public:
    DownloadAction(const QString &serverIp, int port,
                   const QString &command, QObject *parent = 0);

protected slots:
    virtual void readResponse();

private:
    int _fileSize;
    int _receivingBytes;
    QString _structureFilePath;

};

class UploadAction: public LongAction
{
    Q_OBJECT

public:
    UploadAction(const QString &serverIp, int port,
                 const QString &command, const QString &filePath, QObject *parent = 0);

protected slots:
    virtual void readResponse();

private:
    void uploadFile(QTcpSocket *clientSocket);
};
