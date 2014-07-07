#pragma once

#include <QThread>
#include <QDir>

class QFileSystemWatcher;
class QXmlStreamWriter;
class Logger;

class DirScanner: public QThread
{
    Q_OBJECT

public:
    DirScanner(const QString &fileName, const QString &path, QFileSystemWatcher *watcher);
    ~DirScanner();
    void run();

signals:
    void scanned();

private:
    void parseDir(const QDir &);

private:
    Logger * _logger;

    QString _fileName;
    QString _initialPath;
    QFileSystemWatcher * _systemWatcher;
    QXmlStreamWriter * _writerStructureFile;
    QString _logPath;
};
