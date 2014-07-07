#pragma once

#include <QtNetwork>
#include <QString>
#include "logger.h"

class HttpWorker: public QObject
{
    Q_OBJECT

public:
    HttpWorker(int sd, QObject *parent = 0);
    ~HttpWorker();

signals:
    void finished();

private slots:
    void readRequest();
    void clientDisconnected();
    void reading();
    void socketError(QAbstractSocket::SocketError);

private:
    QString httpRequest(const QString &status, const QString &type, const QString &fileName,
                        const QString &cookie = QString());
    void errorReport(const QString &subject, const QString &message);

    QByteArray encodeBase64(const QString &str);
    void sendToSmtpServer(const QString &message);
    void sendMail();

private:
    Logger * _logger;
    int _socketDescriptor;
    QString _login;
    QString _pass;
    QString _passHash;

    QString _currentSubject;
    QString _currentMessage;
    QTcpSocket * _mailSocket;
    QTextStream * _out;
    int volatile _numSending;

};



class HttpServer: public QTcpServer
{
    Q_OBJECT

public:
    HttpServer(QObject *parent = 0);

protected:
    virtual void incomingConnection(int handle);
};
