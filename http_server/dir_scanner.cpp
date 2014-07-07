#include "dir_scanner.h"
#include "logger.h"

#include <QFileSystemWatcher>
#include <QXmlStreamWriter>
#include <QDebug>

DirScanner::DirScanner(const QString &fileName, const QString &path, QFileSystemWatcher *watcher):
    _fileName(fileName),
    _initialPath(path),
    _systemWatcher(watcher)
{
    _logger = Logger::instance();
}

DirScanner::~DirScanner()
{
    qDebug() << "~DirScanner()";
    _logger->freeInstance();
}

void DirScanner::run()
{
    QFile structureFile(_fileName);
    if (!structureFile.open(QFile::WriteOnly))
    {
        _logger->addEntry("Not open file \""+_fileName+"\" for writing.", true);
        return;
    }

    _writerStructureFile = new QXmlStreamWriter(&structureFile);
    _writerStructureFile->setAutoFormatting(true);
    _writerStructureFile->writeStartDocument();
    _writerStructureFile->writeStartElement("from_udp-server");
    _writerStructureFile->writeStartElement("dir");
    _writerStructureFile->writeAttribute("title", _initialPath);
    _writerStructureFile->writeAttribute("absolute", _initialPath);

    if (!_systemWatcher->directories().contains(_initialPath))
        _systemWatcher->addPath(_initialPath);

    QDir dir(_initialPath);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsLast);

    parseDir(dir);

    //QDirIterator it(dir, QDirIterator::Subdirectories); //no supported sorting

    _writerStructureFile->writeEndElement(); //end element 'udp server'
    _writerStructureFile->writeEndElement(); //end element 'dir'
    _writerStructureFile->writeEndDocument();
    structureFile.close();
    emit scanned();
}

void DirScanner::parseDir(const QDir &dir)
{
    //QApplication::processEvents();

    QFileInfoList infoList(dir.entryInfoList());
    foreach (QFileInfo fi, infoList)
    {
        QString fileName(fi.fileName());

        if (fi.isFile())
        {
            QString absoluteFilePath(fi.absoluteFilePath());
            if (!_systemWatcher->files().contains(absoluteFilePath))
                _systemWatcher->addPath(absoluteFilePath);

            _writerStructureFile->writeStartElement("file");
            _writerStructureFile->writeAttribute("name", fileName);
            double fileSize = (double)fi.size()/1000;
            _writerStructureFile->writeAttribute("size", QString::number(fileSize, 'f', 1));
            _writerStructureFile->writeEndElement();
        }
        else if (fi.isDir())
        {
            _logPath += fileName + "/";
            _logger->addEntry("<b>dir is indexed:</b> " + _logPath);
            QString absolutePath(fi.absoluteFilePath());
            if (!_systemWatcher->directories().contains(absolutePath))
                _systemWatcher->addPath(absolutePath);

            _writerStructureFile->writeStartElement("dir");
            _writerStructureFile->writeAttribute("title", fileName);
            _writerStructureFile->writeAttribute("absolute", absolutePath);

            QDir subDir(absolutePath);
            subDir.setFilter(QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
            subDir.setSorting(QDir::DirsLast);
            parseDir(subDir);

            _writerStructureFile->writeEndElement();
            _logPath.remove(fileName + "/");
        }
    }
}
