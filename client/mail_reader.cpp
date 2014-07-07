#include "mail_reader.h"

#include <QtNetwork/QSslCipher>
#include <QStringList>

MailReader::MailReader(int messageNum, QObject *parent):
    QObject(parent),
    _messageNum(messageNum),
    _currentMessageNum(0),
    _popSessionState(0)/*,
    _parseState(0)*/
{
    _sslSocket = new QSslSocket(this);
    connect(_sslSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
                        SLOT(socketStateChanged(QAbstractSocket::SocketState)));

    connect(_sslSocket, SIGNAL(encrypted()),                    SLOT(socketEncrypted()));
    connect(_sslSocket, SIGNAL(sslErrors(QList<QSslError>)),    SLOT(sslErrors(QList<QSslError>)));
    connect(_sslSocket, SIGNAL(readyRead()),                    SLOT(socketReadyRead()));

    _sslSocket->connectToHostEncrypted("pop.gmail.com", 995);
}

void MailReader::socketStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::UnconnectedState)
    {
        _sslSocket->deleteLater();
        _sslSocket = 0;
        //emit finished(_currentMessageNum);
    }
}

void MailReader::socketEncrypted()
{
    if (!_sslSocket)
        return;

    QSslCipher ciph = _sslSocket->sessionCipher();
    QString cipher = QString("%1, %2 (%3/%4)").arg(ciph.authenticationMethod())
                     .arg(ciph.name()).arg(ciph.usedBits()).arg(ciph.supportedBits());

    emit textCipherChanged(cipher);
}

void MailReader::socketReadyRead()
{
    QString data = QString::fromUtf8(_sslSocket->readLine());

    if (_popSessionState < ParseSubject && _messageNum > 0)
        emit appendString(data);

    switch (_popSessionState)
    {
    case SendUser:
        sendData("USER maratka24@gmail.com");
        _popSessionState = SendPass;
        break;

    case SendPass:
        sendData("PASS your_pwd");
        _popSessionState = SendStat;
        break;

    case SendStat:
        sendData("STAT");
        _popSessionState = SendRetr;
        break;

    case SendRetr:
    {
        if (_currentMessageNum == 0)
        {
            QRegExp rxStat("(\\w+)(\\s+)(\\d+)");
            int pos = rxStat.indexIn(data);
            if (pos > -1)
                _currentMessageNum = rxStat.cap(3).toInt();
        }

        if (_messageNum == 0)
        {
            _sslSocket->deleteLater();
            emit finished(_currentMessageNum);
        }
        else if (_messageNum <= _currentMessageNum)// <
        {
            //++_messageNum;
            sendData("RETR " + QString::number(_messageNum));
            _popSessionState = ParseSubject;
        }

        break;
    }

    case ParseSubject:
    {
        do {

            if (data.startsWith("Subject"))
            {
                QStringList dataTokens = data.split(QRegExp("[ \r][ \r]*"));
                QRegExp regExpSubject("=\\?(\\S+)\\?.\\?(\\S+)\\?="); //end - =\\?=
                if (regExpSubject.indexIn(dataTokens[1]) > -1)
                    _subject = decodeBase64(regExpSubject.cap(2));

                qDebug() << "subject " << _subject;
                _popSessionState = ParseContentType;
                break;
            }

            data = QString::fromUtf8(_sslSocket->readLine());
        } while (_sslSocket->canReadLine());

        break;
    }

    case ParseContentType:
    {
        do {

            if (data.startsWith("Content-Type"))
            {
                QStringList dataTokens = data.split(QRegExp("[ \r][ \r]*"));
                if (dataTokens[1].contains("text/plain"))
                {
                    _popSessionState = ParseBodyMessage;
                    break;
                }
            }

            data = QString::fromUtf8(_sslSocket->readLine());
        } while (_sslSocket->canReadLine());

        break;
    }

    case ParseBodyMessage:
    {
        do {

            if (!data.startsWith("Content-Transfer"))
            {
                data = data.simplified();
                if (!data.isEmpty())
                {
                    _body += decodeBase64(data);
                    emit messageReceived(_subject, _body);
                    _popSessionState = MessageReceived;
                    break;
                }
            }

            data = QString::fromUtf8(_sslSocket->readLine());
        } while (_sslSocket->canReadLine());

        break;
    }

    case MessageReceived:
    {
        if (_messageNum < _currentMessageNum)
        {
            ++_messageNum;
            sendData("RETR " + QString::number(_messageNum));
            _popSessionState = ParseSubject;
        }
        else
        {
            qDebug() << "finish " << data;
            _sslSocket->deleteLater();
            emit finished(_currentMessageNum);
            _popSessionState = -1;
        }

        break;
    }

//    default:
//        _sslSocket->deleteLater();
//        emit finished(_currentMessageNum);
//        break;

    }
}

void MailReader::sendData(QString data)
{
    emit appendString(data + '\n');
    _sslSocket->write(data.toUtf8() + "\r\n");
}

QString MailReader::decodeBase64(const QString &str)
{
    QByteArray data;
    data.append(str);
    QByteArray precode(QByteArray::fromBase64(data));
    return QString::fromUtf8(precode.constData());
}

void MailReader::sslErrors(const QList<QSslError> &errors)
{
    foreach (const QSslError &error, errors)
        emit appendString(error.errorString());

    if (_sslSocket->state() != QAbstractSocket::ConnectedState)
        socketStateChanged(_sslSocket->state());
}
