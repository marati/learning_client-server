#pragma once

#include <QTextStream>
#include <QFile>

class Logger
{
public:
    static Logger* instance();

    void freeInstance();

    void addEntry(QString textEntry, bool isError = false);

protected:
    Logger();

private:
    //TODO: use QScopedPointer
    static Logger * _self;
    static int _refCount;

    QFile * _logFile;
    QTextStream * _fileWriter;
};
