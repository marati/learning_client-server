#include "drop_list_widget.h"

#include <QUrl>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDebug>

DropListWidget::DropListWidget(QWidget *parent):
    QListWidget(parent)
{
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setAcceptDrops(true);
}

void DropListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    event->accept();
}

void DropListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

void DropListWidget::dropEvent(QDropEvent *event)
{
    QList<QUrl> urlList = event->mimeData()->urls();
    QString str;

    foreach (QUrl url, urlList)
        str += url.toString();

    emit dropped(str);
}
