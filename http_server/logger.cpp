#include "logger.h"

#include <QDir>
#include <QDebug>

Logger * Logger::_self = 0;
int Logger::_refCount = 0;

Logger::Logger()
{
    _logFile = new QFile("log.txt");

    QDir dir(QDir::currentPath());
    dir.remove("log.txt");


    if (_logFile->open(QFile::Append | QFile::Text))
        _fileWriter = new QTextStream(_logFile);
}

Logger* Logger::instance()
{
    if (!_self)
        _self = new Logger;

    ++_refCount;
    return _self;
}

void Logger::freeInstance()
{
    _refCount--;

    qDebug() << "ref -- " << _refCount;

    if (!_refCount)
    {
        _fileWriter->device()->close();
        delete this;
        _self = 0;
    }
}

void Logger::addEntry(QString textEntry, bool isError)
{
    QTextStream out(stdout);

    if (isError)
        textEntry = "(-)" + textEntry;
    else
        textEntry = "(+)" + textEntry;

    out << textEntry << endl;
    *_fileWriter << textEntry << endl;

    out.flush();
}
