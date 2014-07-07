#pragma once

#include <QListWidget>

class QDragEnterEvent;
class QDropEvent;

class DropListWidget: public QListWidget
{
    Q_OBJECT

public:
    DropListWidget(QWidget *parent = 0);

signals:
    void dropped(const QString &);

protected:
    virtual void dragMoveEvent(QDragMoveEvent *event);
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dropEvent(QDropEvent *event);

};
