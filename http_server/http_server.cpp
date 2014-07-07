#include "http_server.h"

#include <QDebug>

HttpWorker::HttpWorker(int sd, QObject *parent):
    QObject(parent),
    _socketDescriptor(sd),
    _numSending(0)
{
    _logger = Logger::instance();

    int randPort = 0;
    while (randPort < 1025)
    {
        qsrand(QDateTime::currentDateTime().toTime_t() + int(this));
        randPort = qrand()%10000;
    }

    QTcpSocket *clientSocket = new QTcpSocket(this);
    if (!clientSocket->setSocketDescriptor(_socketDescriptor))
        _logger->addEntry(clientSocket->errorString(), true);

    connect(clientSocket, SIGNAL( readyRead() ),     SLOT( readRequest() ));
    connect(clientSocket, SIGNAL( disconnected() ),  SLOT( clientDisconnected() ));

    _mailSocket = new QTcpSocket(this);
    connect(_mailSocket, SIGNAL( readyRead() ), SLOT( reading() ));
    connect(_mailSocket, SIGNAL( error(QAbstractSocket::SocketError) ), SLOT( socketError(QAbstractSocket::SocketError) ));

    _logger->addEntry("Http Server: connected! IP: " + clientSocket->peerAddress().toString());

    //access to server, login == email
    _login = "mail4lab@inbox.ru";
    _pass = "1234test";

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(_pass.toAscii());
    _passHash = hash.result().toHex();
}

HttpWorker::~HttpWorker()
{
    _logger->freeInstance();
}

QString HttpWorker::httpRequest(const QString &status, const QString &type, const QString &fileName,
                                const QString &cookie)
{
    QFile file;
    QString disposeHeader;

    if (type == "application/octet-stream")// application/x-executable
    {
        file.setFileName(fileName);
        disposeHeader = "Content-Disposition: attachment; filename=\""+ fileName +"\"";
    }
    else
        file.setFileName(QDir::currentPath() + "/data/" + fileName);

    if (!file.open(QFile::ReadOnly))
    {
        _logger->addEntry("Not open file for sending " + fileName, true);
        return "";
    }

    int length = file.size();
    QString request =
            "HTTP/1.1 " + status + "\r\n"
            "Content-Type: " + type + "\r\n";

    if (!cookie.isEmpty())
    {
        request += "Set-Cookie: " + cookie + "\r\n";
    }

    if (!disposeHeader.isEmpty())
    {
        request += disposeHeader + "\r\n"
                + "Content-Transfer-Encoding: binary\r\n"
                + "Content-Length: " + QString::number(length) + "\r\n";
    }
    else
    {
        request += "Content-Length: " + QString::number(length) + "\r\n"
                   "\r\n" + QString::fromUtf8(file.readAll());
    }

    qDebug() << "final :" << request;
    return request;
}

void HttpWorker::errorReport(const QString &subject, const QString &message)
{
    qDebug() << "err rep " << message;
    _currentSubject = subject;
    _currentMessage = message;

    if (_mailSocket->state() == QAbstractSocket::UnconnectedState)
    {
        _mailSocket->connectToHost("smtp.mail.ru", 587);
        _mailSocket->waitForConnected(3000);
        _out = new QTextStream(_mailSocket);
        return;
    }

    _numSending = 5;
    sendMail();
}

void HttpWorker::readRequest()
{

    QTcpSocket *clientSocket = (QTcpSocket*)sender();
    if (clientSocket->canReadLine())
    {
        QTextStream os(clientSocket);
        os.setAutoDetectUnicode(true);

        QString line = clientSocket->readLine();
        QStringList tokens = line.split(QRegExp("[ \r\n][ \r\n]*"));
        qDebug() << "LINE and TOKENS"<< tokens << line;

        if (tokens[0] == "GET")
        {
            bool requiredCookie = false;
            while (clientSocket->canReadLine())
            {
                QStringList tokens = QString(clientSocket->readLine()).split(QRegExp("[ \r\n][ \r\n]*"));

                if (tokens[0].contains("Cookie"))
                {
                    QStringList cookie = tokens[1].split("=");

                    if (cookie[0] == _login && cookie[1] == _passHash)
                        requiredCookie = true;
                    else
                    {
                        os << httpRequest("200 OK", "text/html; charset=\"utf-8\"",  "login-error.html");//подмена cookie

                        errorReport(QString::fromUtf8("Неправильные cookies"),
                                    QString::fromUtf8("Логин или пароль, содержащийся в куках неверны"));
                        clientSocket->close();
                        return;
                    }

                    break;
                }
            }

            QStringList pathTokens = tokens[1].split("/", QString::SkipEmptyParts);

            if (pathTokens[0].contains(".ico"))
            {
                //отдать фавиконку
                return;
            }

            if (!requiredCookie)
            {
                QRegExp loginRegExp("login.htm(l?)");
                if (pathTokens.isEmpty() || loginRegExp.exactMatch(pathTokens[0]))
                    os << httpRequest("200 OK", "text/html; charset=\"utf-8\"",  "login.html");
                else
                {
                    os << httpRequest("404 Not Found", "text/html; charset=\"utf-8\"", "not-found.html");
                    errorReport("Page Not Found",
                                QString::fromUtf8("Запрошенная Вами страница ") + pathTokens[0] + QString::fromUtf8(" не найдена."));
                }
                //clientSocket->close();
                return;
            }

            if (pathTokens.isEmpty())
            {
                os << httpRequest("200 OK", "text/html; charset=\"utf-8\"", "index.html");
            }
            else if (pathTokens[0].contains(".htm"))
            {
                if (QFile::exists(QDir::currentPath() + "/data/" + pathTokens[0]))
                    os << httpRequest("200 OK", "text/html; charset=\"utf-8\"",  pathTokens[0]);
                else
                {
                    os << httpRequest("404 Not Found", "text/html; charset=\"utf-8\"", "not-found.html");
                    errorReport("Page Not Found",
                                QString::fromUtf8("Запрошенная Вами страница ") + pathTokens[0] + QString::fromUtf8(" не найдена."));
                }
            }
            else
            {
                if (QFile::exists(tokens[1]))
                {
                     os << httpRequest("200 OK", "application/octet-stream",  tokens[1]);// application/x-executable

                     QFile fileToSend(tokens[1]);
                     if (!fileToSend.open(QFile::ReadOnly))
                     {
                         _logger->addEntry("Not open file \""+tokens[1]+"\" for writing.", true);
                         return;
                     }

                     quint64 pos = 0;
                     quint64 fileSize = fileToSend.size();
                     quint64 partSize = 1024;
                     QDataStream out(clientSocket);

                     while (pos < fileSize)
                     {
                         int blockSize = qMin(partSize, fileSize - pos);
                         char data[blockSize];
                         QDataStream fromFile(&fileToSend);
                         int outData = fromFile.readRawData(data, sizeof(char) * (blockSize));
                         out.writeRawData(data, sizeof(char)*outData);
                         pos += blockSize;
                     }
                     out.device()->close();
                     fileToSend.close();

                }
                else
                {
                    os << httpRequest("404 Not Found", "text/html; charset=\"utf-8\"", "not-found.html");
                    errorReport("Page Not Found",
                                QString::fromUtf8("Запрошенный Вами файл ") + tokens[1] + QString::fromUtf8(" не найден."));
                }
            }
            //clientSocket->close();

        }
        else if (tokens[0] == "POST")
        {
            //read Request Headers
            while (clientSocket->canReadLine())
                QStringList headerTokens = QString(clientSocket->readLine()).split(QRegExp("[ \r\n][ \r\n]*"));

            QTextStream os(clientSocket);
            os.setAutoDetectUnicode(true);
            //read Form data
            QStringList formDataTokens = QString(clientSocket->readLine()).split(QRegExp("[ \r\n][ \r\n]*"));
            QStringList authInfo = formDataTokens[0].split("&");

            QString receivedLogin = authInfo[0].remove("username=") + "@inbox.ru";
            QString pass = authInfo[1].remove("password=");

            if (_login == receivedLogin && _pass == pass)
            {
                QString cookie = _login + "=" + _passHash;
                os << httpRequest("200 OK", "text/html; charset=\"utf-8\"", "index.html", cookie);
            }
            else
            {
                os << httpRequest("200 OK", "text/html; charset=\"utf-8\"", "login-error.html");
                errorReport(QString::fromUtf8("Ошибка аутентификации"),
                            QString::fromUtf8("Введённая Вами пара логин/пароль неверна: (") + receivedLogin + " / " + pass + ")");
            }
        }
    }
}

void HttpWorker::clientDisconnected()
{
    QTcpSocket *clientSocket = (QTcpSocket*)sender();
    _logger->addEntry("Client (" + clientSocket->peerAddress().toString() + ") disconnected.");
    clientSocket->deleteLater();
    emit finished();
}

void HttpWorker::reading()
{
    QTcpSocket *mailSocket = (QTcpSocket*)sender();
    QTextStream in(mailSocket);

    QString line;
    while (!in.atEnd())
    {
        line = in.readLine();
        _logger->addEntry(line);
    }

    qDebug() << "reading" << line;

    if (line.contains("closing"))
    {
        _mailSocket->close();
        _numSending = 0;
    }

    ++_numSending;
    sendMail();
}

QByteArray HttpWorker::encodeBase64(const QString &str)
{
    QByteArray text;
    text.append(str);
    return text.toBase64();
}

void HttpWorker::sendToSmtpServer(const QString &message)
{
    *_out << message + "\r\n";
    _out->flush();
}

void HttpWorker::sendMail()
{
    qDebug() << "sendMail" << _currentSubject << _currentMessage << _numSending << _mailSocket->state();

    switch(_numSending)
    {
    case 1:
        sendToSmtpServer("HELO marat");
        break;

    case 2:
        sendToSmtpServer("AUTH LOGIN");
        break;

    case 3:
        sendToSmtpServer(encodeBase64(_login));
        break;

    case 4:
        sendToSmtpServer(encodeBase64(_pass));
        break;

    case 5:
        // sending From Email Address
        sendToSmtpServer("MAIL FROM: <" + _login + ">");
        break;

    case 6:
        // sending To Email Address
        sendToSmtpServer("RCPT TO: <" + QString("maratka24@gmail.com") + ">");
        break;

    case 7:
        sendToSmtpServer("DATA");
        break;

    case 8:
        sendToSmtpServer("SUBJECT: " + _currentSubject + "\r\n" +
                         _currentMessage + "\r\n" + ".");
        break;

    }
}

void HttpWorker::socketError(QAbstractSocket::SocketError err)
{
    QTcpSocket *socket = (QTcpSocket*)sender();
    QString strError =
        "Error: " + (err == QAbstractSocket::HostNotFoundError ?
                     "The host was not found." :
                     err == QAbstractSocket::RemoteHostClosedError ?
                     "The remote host is closed." :
                     err == QAbstractSocket::ConnectionRefusedError ?
                     "The connection was refused." :
                     QString(socket->errorString())
                    );
    _logger->addEntry(strError, true);
}



HttpServer::HttpServer(QObject *parent):
    QTcpServer(parent)
{
}

void HttpServer::incomingConnection(int handle)
{
    HttpWorker *worker = new HttpWorker(handle);
    QThread *serverThread  = new QThread;
    worker->moveToThread(serverThread);

    serverThread->connect(worker, SIGNAL( finished() ), SLOT( quit() ));
    serverThread->start();
}
