#pragma once

#include <QObject>
#include <QSslSocket>
#include <QtNetwork/QAbstractSocket>

class MailReader: public QObject
{
    Q_OBJECT

public:
    MailReader(int messageNum = 0, QObject *parent = 0);

signals:
    void textCipherChanged(const QString &cipher);
    void appendString(const QString &line);
    void messageReceived(const QString &subject, const QString &body);
    void finished(int);

private slots:
    void socketStateChanged(QAbstractSocket::SocketState state);
    void socketEncrypted();
    void socketReadyRead();
    void sslErrors(const QList<QSslError> &errors);

private:
    void sendData(QString);
    QString decodeBase64(const QString &);

private:
    QSslSocket * _sslSocket;
    int _messageNum;
    int _currentMessageNum;

    int _popSessionState;
    enum PopSessionState
    {
        SendUser,
        SendPass,
        SendStat,
        SendRetr,
        ParseSubject,
        ParseContentType,
        ParseBodyMessage,
        MessageReceived
    };

//    int _parseState;
//    enum ParseState
//    {
//        SubjectState,
//        ContentTypeState,
//        BodyMessageState
//    };

    QString _subject;
    QString _body;

};
