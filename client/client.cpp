#include "client.h"
#include "ui_client.h"
#include "file_model.h"
#include "long_actions.h"
#include "mail_reader.h"

#include <QDomDocument>
#include <QListWidgetItem>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>

Client::Client(QFrame *parent):
    QFrame(parent),
    _ui(new Ui::Client),
    _nextBlockSize(0),
    _messageNum(0),
    _responseCode(0)
{
    _ui->setupUi(this);
    tuneWidgets();

    _udpSocket = new QUdpSocket(this);
    _udpSocket->bind(15444, QUdpSocket::ShareAddress);//15444 - port for receiving;
    connect(_udpSocket, SIGNAL( readyRead() ), SLOT( startReadingStructure() ));
    connect(_udpSocket,
            SIGNAL(error(QAbstractSocket::SocketError)),
            SLOT(socketError(QAbstractSocket::SocketError)));

    _commandTcpSocket = new QTcpSocket(this);
    connect(_commandTcpSocket, SIGNAL( connected() ), SLOT( connected() ));
    connect(_commandTcpSocket, SIGNAL( disconnected() ), _commandTcpSocket, SLOT( deleteLater() ));
    connect(_commandTcpSocket, SIGNAL( readyRead() ), SLOT( readResponse() ));
    connect(_commandTcpSocket,
            SIGNAL(error(QAbstractSocket::SocketError)),
            SLOT(socketError(QAbstractSocket::SocketError)));

    on_requestMail_clicked();

    //testFileSystemView();
}

Client::~Client()
{
    delete _ui;
}

void Client::tuneWidgets()
{
    _ui->fileSystem->header()->setResizeMode(QHeaderView::Stretch);
    connect(_ui->fileSystem, SIGNAL( expanded(QModelIndex) ),   SLOT( addExpandedIndex(QModelIndex) ));
    connect(_ui->fileSystem, SIGNAL( collapsed(QModelIndex) ),  SLOT( removeExpandedIndex(QModelIndex) ));

    connect(_ui->requestStructure, SIGNAL( clicked() ), SLOT( requestStructure() ));

    connect(_ui->ftpWidget, SIGNAL(customContextMenuRequested(QPoint)), SLOT( showContextMenu() ));
    connect(_ui->ftpWidget, SIGNAL(doubleClicked(QModelIndex)), SLOT(changeOrDisplayDir(QModelIndex)));
    connect(_ui->ftpWidget, SIGNAL(dropped(QString)), SLOT(requestUploadFile(QString)));

    //_ui->logTable->horizontalHeader()->setResizeMode(QHeaderView::Stretch);

    connect(_ui->listMail, SIGNAL(clicked(QModelIndex)), SLOT(showMessage(QModelIndex)));
    connect(_ui->listMail,
            SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            SLOT(showMessageForCurrentItem(QListWidgetItem*,QListWidgetItem*)));

    setWindowTitle("Client");
    setMinimumWidth(width()/1.5);
}

void Client::connected()
{
    addLog("Client connected to server");
}

void Client::addExpandedIndex(const QModelIndex &newIndex)
{
    foreach(QModelIndex index, _expanded)
        if (index.data(FileModel::PathRole).toString() == newIndex.data(FileModel::PathRole).toString())
            return;

    _expanded << newIndex;
}

void Client::removeExpandedIndex(const QModelIndex &index)
{
    _expanded.removeOne(index);
}

void Client::showContextMenu()
{
    bool connected = _commandTcpSocket->state() == QAbstractSocket::ConnectedState;
    QModelIndex selectedIndex = _ui->ftpWidget->selectionModel()->currentIndex();
    if (!selectedIndex.isValid() || !connected)
        return;

    QList<QAction *> actions;

    QAction *downloadAction = new QAction("Download", _ui->ftpWidget);
    downloadAction->setShortcut(QKeySequence::Save);
    connect(downloadAction, SIGNAL( triggered() ), SLOT( requestDownloadFile() ));
    actions << downloadAction;

    if (!selectedIndex.data(FileModel::PathRole).toString().isEmpty())
    {
        downloadAction->setEnabled(false);
    }

    QAction *createDirAction = new QAction("Create dir", _ui->ftpWidget);
    createDirAction->setShortcut(QKeySequence::New);
    connect(createDirAction, SIGNAL( triggered() ), SLOT( requestCreateDir() ));
    actions << createDirAction;

    QAction *deleteAction = new QAction("Delete", _ui->ftpWidget);
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, SIGNAL( triggered() ), SLOT( requestDelete() ));
    actions << deleteAction;

    QAction *renameAction = new QAction("Rename", _ui->ftpWidget);
    connect(renameAction, SIGNAL( triggered() ), SLOT( requestRename() ));
    actions << renameAction;

    QMenu *contextMenu = new QMenu(this);
    contextMenu->addActions(actions);
    contextMenu->exec(QCursor::pos());
}

void Client::changeOrDisplayDir(const QModelIndex &index)
{
    QString path;
    if (index.isValid())
    {
        path = index.data(Qt::UserRole + 1).toString();
        if (path.isEmpty())
            return;
    }
    else
    {
        if (_expanded.isEmpty())
            path = "*";
        else
            path = _expanded.last().data(FileModel::PathRole).toString();
    }

    _ui->ftpWidget->clear();

    if (path == "..")
    {
        path = _expanded.takeLast().data(FileModel::PathRole).toString();
        QModelIndexList indexes = _fileModel->match(_fileModel->index(0,0), FileModel::PathRole, path, -1,
                                                    Qt::MatchFixedString | Qt::MatchRecursive);
        QModelIndex collapsedIndex = indexes.at(0);
        _ui->fileSystem->setExpanded(collapsedIndex, false);
        _ui->fileSystem->selectionModel()->setCurrentIndex(collapsedIndex, QItemSelectionModel::Deselect);

        displayCurrentDir(collapsedIndex.sibling(0,0));
        return;
    }

    QModelIndex expandingIndex;
    if (_expanded.isEmpty())
    {
        if (path == "*")
        {
            expandingIndex = _fileModel->index(0,0);
            displayCurrentDir(expandingIndex);
            return;
        }
        else
        {
            QModelIndexList indexes = _fileModel->match(_fileModel->index(0,0), FileModel::PathRole, path, -1,
                                                        Qt::MatchFixedString);
            expandingIndex = indexes.at(0);
        }
    }
    else
    {
        QModelIndexList indexes = _fileModel->match(_fileModel->index(0,0), FileModel::PathRole, path, -1,
                                                    Qt::MatchFixedString | Qt::MatchRecursive);
        expandingIndex = indexes.at(0);
    }

    //_ui->fileSystem->selectionModel()->setCurrentIndex(expandingIndex, QItemSelectionModel::Select);

    QModelIndex parentExpanding(expandingIndex);
    do
    {
        qDebug() << "expandd" << parentExpanding.data().toString();
        _ui->fileSystem->setExpanded(parentExpanding, true);
        parentExpanding = parentExpanding.parent();
    } while (parentExpanding.isValid());

    //deselect last expand
//    QItemSelectionModel *selectionModel = _ui->fileSystem->selectionModel();
//    if (selectionModel->currentIndex().isValid())
//        selectionModel->setCurrentIndex(selectionModel->currentIndex(), QItemSelectionModel::Deselect);
    //

    QModelIndex child = expandingIndex.child(0,0);
    displayCurrentDir(child);

//    if (!child.isValid())
//        selectionModel->setCurrentIndex(expandingIndex, QItemSelectionModel::Select);

}

void Client::on_requestMail_clicked()
{
    MailReader *reader = new MailReader(_messageNum);
    QThread *readerThread = new QThread(this);
    reader->moveToThread(readerThread);
    _ui->requestMail->setDisabled(true);

    connect(reader, SIGNAL( textCipherChanged(QString) ),       SLOT( setCipher(QString) ));
    connect(reader, SIGNAL( finished(int) ),                    SLOT( setMessageNum(int) ));
    connect(reader, SIGNAL( messageReceived(QString, QString)), SLOT( updateListMail(QString, QString) ));
    connect(reader, SIGNAL( appendString(QString) ),            SLOT( addLog(QString) ));

    readerThread->connect(reader, SIGNAL( finished(int) ), SLOT( quit() ));
    readerThread->start();
}

void Client::setCipher(const QString &cipher)
{
    _ui->cipher->setText(cipher);
}

void Client::setMessageNum(int num)
{
    _messageNum = num;
    _ui->requestMail->setDisabled(false);
}

void Client::updateListMail(const QString &subject, const QString &message)
{
    QListWidgetItem *messageItem = new QListWidgetItem(_ui->listMail);
    messageItem->setText(subject);
    messageItem->setData(Qt::UserRole + 1, message);
    _ui->listMail->setCurrentItem(messageItem, QItemSelectionModel::Select);
}

void Client::showMessage(const QModelIndex &index)
{
    _ui->textLetter->setText(index.data(Qt::UserRole + 1).toString());
}

void Client::showMessageForCurrentItem(QListWidgetItem *current, QListWidgetItem */*previous*/)
{
   QModelIndex currentIndex(_ui->listMail->model()->index(_ui->listMail->row(current), 0));
   showMessage(currentIndex);
}

void Client::createThread(LongAction *action)
{
    QThread *newThread = new QThread(this);
    action->moveToThread(newThread);

    connect(action, SIGNAL(addToLog(QString, bool)),    SLOT(addLog(QString, bool)));
    connect(action, SIGNAL(finished(QString)),          SLOT(structureDownloadFinished(QString)));

    newThread->connect(action, SIGNAL( finished() ), SLOT( quit() ));
    newThread->start();
}

void Client::startReadingStructure()
{
    while (_udpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(_udpSocket->pendingDatagramSize());

        if (!_udpSocket->readDatagram(datagram.data(), datagram.size(), &_serverIp))
        {
            addLog("Failed reading structure", true);
            return;
        }

        if (_responseCommand.isEmpty())
        {
            QDataStream in(&datagram, QIODevice::ReadOnly);
            in >> _responseCommand;

            if (_responseCommand.isEmpty())
            {
                addLog("Action is not defined", true);
                return;
            }

            if (_responseCommand == "START_STRUCTURE_TRANSFER")
            {
                QDir dir(QDir::currentPath() + "/download_files/");
                QStringList xmlFiles = dir.entryList(QStringList() << "*.xml", QDir::Files);
                foreach (QString xmlFile, xmlFiles)
                    dir.remove(xmlFile);

                int port;
                in >> port;

                _ui->ipEdit->setText(_serverIp.toString());
                _ui->ipEdit->setReadOnly(true);
                _ui->portEdit->setText(QString::number(port));
                _ui->portEdit->setReadOnly(true);

                if (_commandTcpSocket->state() == QAbstractSocket::UnconnectedState)
                {
                    _commandTcpSocket->connectToHost(_serverIp, port);
                    _commandTcpSocket->waitForConnected();
                }

                createThread(new DownloadAction(_serverIp.toString(), port, "GIVE_STRUCTURE"));

                _responseCommand.clear();
            }
        }
    }
}

void Client::readResponse()
{
    QTcpSocket *clientSocket = (QTcpSocket*)sender();
    QDataStream in(clientSocket);

    for (;;)
    {
        if (!_nextBlockSize)
        {
            if (clientSocket->bytesAvailable() < sizeof(quint32))
                break;

            in >> _nextBlockSize;
        }

        if (clientSocket->bytesAvailable() < _nextBlockSize)
            break;

        if (!_responseCode)
        {
            in >> _responseCode;

            QString info;

            switch (_responseCode)
            {

            case 257:
            case 250:
                in >> info;
                break;

            case 350:
            {
                in >> info;
                QTextStream out(clientSocket);
                out << _responseServer;
                out.flush();
                break;
            }

            case 450:
            case 451:
                in >> info;
                break;

            }

            QString response = "Response: " + QString::number(_responseCode);
            if (!info.isEmpty())
                response += ": " + info;

            addLog(response);

            _responseCode = 0;
            _nextBlockSize = 0;
            continue;
        }
    }
}

void Client::requestStructure()
{
    QString ip = _ui->ipEdit->text();
    QString port = _ui->portEdit->text();

    if (_commandTcpSocket->state() == QAbstractSocket::UnconnectedState)
    {
        if (ip.isEmpty() || port.isEmpty())
        {
            QMessageBox::information(_ui->ftpWidget,
                                    "Fill in all fields",
                                    "Fields ip and port is required");
            return;
        }

        QHostAddress ha;
        if (!ha.setAddress(ip))
        {
            QMessageBox::information(_ui->ftpWidget,
                                    "IP Address is not valid",
                                    "Enter valid IP");
            return;
        }

        _commandTcpSocket->connectToHost(ip, port.toInt());
    }

    createThread(new DownloadAction(ip, port.toInt(), "GIVE_STRUCTURE"));
}

void Client::setExpandState(bool expanded)
{
    foreach (QModelIndex index, _expanded)
        _ui->fileSystem->setExpanded(index, expanded);
}

bool Client::displayFileStructure(QIODevice *device)
{
    QString errorStr;
    int errorLine;
    int errorColumn;

    if (!_domDocument.setContent(device, true, &errorStr, &errorLine, &errorColumn))
    {
        QMessageBox::information(window(), tr("UDP client"),
                                 tr("Parse error at line %1, column %2:\n%3")
                                 .arg(errorLine)
                                 .arg(errorColumn)
                                 .arg(errorStr));
        return false;
    }

    QDomElement root = _domDocument.documentElement();
    QDomNode child = root.firstChild();
    QDomNamedNodeMap attributeMap = child.attributes();

    QString newRootPath(attributeMap.namedItem("title").nodeValue());
    window()->setWindowTitle(newRootPath);

    _fileModel = new FileModel(_domDocument, this);
    _ui->fileSystem->setModel(_fileModel);

    if (_rootPath == newRootPath)
    {
        setExpandState(true);
        changeOrDisplayDir();
    }
    else
    {
        setExpandState(false);
        _expanded.clear();
        _rootPath = newRootPath;
        changeOrDisplayDir();
    }

    device->close();
    return true;
}

void Client::testFileSystemView()
{
    QFile acceptFile("structure-from-server.xml");
    if (!acceptFile.open(QFile::ReadOnly))
        addLog("Not open file \"structure-from-server.xml\" for writing.", true);

    displayFileStructure(&acceptFile);
}

void Client::displayCurrentDir(QModelIndex current)
{
    setWindowTitle(currentPath());

    if (!_expanded.isEmpty() || !current.isValid())
    {
        QListWidgetItem *cdUpItem = new QListWidgetItem(_ui->ftpWidget);
        cdUpItem->setText("..");
        cdUpItem->setData(Qt::UserRole + 1, "..");

//        if (current.isValid())
//            _ui->fileSystem->selectionModel()->setCurrentIndex(current.parent(), QItemSelectionModel::Select);
    }

    int row = 1;

    do
    {
        QListWidgetItem *item = new QListWidgetItem(_ui->ftpWidget);
        item->setIcon(current.data(Qt::DecorationRole).value<QIcon>());
        item->setText(current.data().toString());

        QString path = current.data(FileModel::PathRole).toString();
        if (!path.isEmpty())
            item->setData(Qt::UserRole + 1, path);

        current = current.sibling(row, 0);
        ++row;
    } while (current.isValid());
}

void Client::structureDownloadFinished(const QString &fileName)
{
    if (fileName.isEmpty())
        return;

    QFile structure(fileName);
    if (!structure.open(QFile::ReadOnly))
    {
        addLog("Error open file " + fileName + "", true);
        return;
    }
    displayFileStructure(&structure);
}

bool Client::checkUniqueItem(const QString &itemText)
{
    QList<QListWidgetItem *> items =
            _ui->ftpWidget->findItems("*", Qt::MatchWildcard | Qt::MatchWrap);

    foreach (QListWidgetItem *item, items)
        if (item->text() == itemText)
            return false;

    return true;
}

QString Client::currentPath()
{
    QString path;

    if (_expanded.isEmpty())
        path = _rootPath;
    else
        path = _expanded.last().data(FileModel::PathRole).toString();

    return path;
}

void Client::requestDownloadFile()
{
    QModelIndex selectedIndex = _ui->ftpWidget->selectionModel()->currentIndex();

    QString path = selectedIndex.data(Qt::UserRole + 1).toString();
    QString fileName = selectedIndex.data().toString();

    if (path.isEmpty())
        path = currentPath();


    QString command = "RETR " + path + "/" + fileName;
    createThread(new DownloadAction(_ui->ipEdit->text(), _ui->portEdit->text().toInt(), command));
}

void Client::requestUploadFile(QString filePath)
{
    filePath = filePath.remove("file://");
    if (!QFile::exists(filePath))
    {
        addLog("File " + filePath + " can't open for writing", true);
        return;
    }

    UploadAction *upload = new UploadAction(_ui->ipEdit->text(), _ui->portEdit->text().toInt(),
                                            "CWD " + currentPath(), filePath);
    createThread(upload);
}

void Client::requestCreateDir()
{
    bool inputted;
    QString dirName = QInputDialog::getText(
                _ui->ftpWidget,
                "Input dir name",
                "",
                QLineEdit::Normal,
                "New folder",
                &inputted);

    if (!inputted)
        return;

    if (!checkUniqueItem(dirName))
    {
        addLog("Enter a unique dir name", true);
        return;
    }

    QString path = currentPath();

    qDebug() << "REQ_MKDIR" << path << dirName;

    QTextStream out(_commandTcpSocket);
    QString command = "MKD ";
    QString sendpath = path + "/" + dirName;
    out << command << sendpath;
    out.flush();
}

void Client::requestDelete()
{
    QModelIndex selectedIndex = _ui->ftpWidget->selectionModel()->currentIndex();
    QString command;

    //delete path
    QString path = selectedIndex.data(Qt::UserRole + 1).toString();

    //delete file
    if (path.isEmpty())
    {
        path = currentPath();
        path += "/" + selectedIndex.data().toString();
        command = "DELE";
    }
    else
    {
        command = "RMD";
    }

    qDebug() << "REQ_RM" << command + " " + path;

    QTextStream out(_commandTcpSocket);
    out << command + " " + path;
    out.flush();
}

void Client::requestRename()
{
    QModelIndex selectedIndex = _ui->ftpWidget->selectionModel()->currentIndex();
    QString oldFileName = selectedIndex.data().toString();

    bool inputted;
    QString newFileName = QInputDialog::getText(
                _ui->ftpWidget,
                "Input new file name",
                "",
                QLineEdit::Normal,
                oldFileName,
                &inputted);

    if (!inputted)
        return;

    if (!checkUniqueItem(newFileName))
    {
        addLog("Enter a unique file name", true);
        return;
    }

    QString path = currentPath();
    path += "/" + oldFileName;

    QTextStream out(_commandTcpSocket);
    out << "RNFR " + path;
    out.flush();

    _responseServer = "RNTO " + currentPath() + "/" + newFileName;
}

void Client::addLog(const QString &logStr, bool isError)
{
    int row = _ui->logTable->rowCount();
    _ui->logTable->insertRow(row);

    QString appendStr = isError ? "<font color=red>" + logStr + "</font>" : logStr;

    QLabel *logLable = new QLabel(appendStr);
    logLable->setWordWrap(true);
    _ui->logTable->setCellWidget(row, 0, logLable);
    _ui->logTable->scrollToBottom();
}

void Client::socketError(QAbstractSocket::SocketError err)
{
    QAbstractSocket *socket = (QAbstractSocket*)sender();
    QString strError =
        "Error: " + (err == QAbstractSocket::HostNotFoundError ?
                     "The host was not found." :
                     err == QAbstractSocket::RemoteHostClosedError ?
                     "The remote host is closed." :
                     err == QAbstractSocket::ConnectionRefusedError ?
                     "The connection was refused." :
                     QString(socket->errorString())
                    );
    addLog(strError, true);
}
