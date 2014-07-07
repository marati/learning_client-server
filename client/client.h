#pragma once

#include <QtNetwork>
#include <QFrame>
#include <QDomDocument>

namespace Ui
{
    class Client;
}

class QListWidgetItem;
class LongAction;
class FileModel;

class Client: public QFrame
{
    Q_OBJECT

public:
    Client(QFrame *parent = 0);
    ~Client();

private slots:
    void connected();
    void socketError(QAbstractSocket::SocketError);

    //workaround of mail (ui)
    void on_requestMail_clicked();
    void setCipher(const QString &);
    void setMessageNum(int);
    void updateListMail(const QString &subject, const QString &message);
    void showMessage(const QModelIndex &index = QModelIndex());
    void showMessageForCurrentItem(QListWidgetItem *current, QListWidgetItem *previous);
    //

    //working with fileSystem widget
    void addExpandedIndex(const QModelIndex &newIndex);
    void removeExpandedIndex(const QModelIndex &);
    //

    void showContextMenu();
    void changeOrDisplayDir(const QModelIndex &index = QModelIndex());

    //receive udp datagram to take download starts
    void startReadingStructure();
    void readResponse();

    //requests to client
    void requestStructure();
    void structureDownloadFinished(const QString &fileName);
    void requestDownloadFile();
    void requestUploadFile(QString filePath);
    void requestCreateDir();
    void requestDelete();
    void requestRename();
    //end requests

    void addLog(const QString &, bool isError = false);

private:
    void tuneWidgets();
    void createThread(LongAction *);

    void setExpandState(bool expanded);
    bool displayFileStructure(QIODevice *);
    void testFileSystemView(); //test function
    void displayCurrentDir(QModelIndex current);

    //help functions for requests
    bool checkUniqueItem(const QString &itemText);
    QString currentPath();
    //

private:
    Ui::Client *_ui;
    QUdpSocket * _udpSocket;
    QTcpSocket * _commandTcpSocket;
    QHostAddress _serverIp;

    int _nextBlockSize;
    int _messageNum;

    FileModel * _fileModel;
    QDomDocument _domDocument;
    QString _rootPath;
    QModelIndexList _expanded;
    //QHash<QListWidgetItem *, QDomElement> _domElementForItem;

    QString _responseCommand;
    int _responseCode;
    QString _responseServer;

};
