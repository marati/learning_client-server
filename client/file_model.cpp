#include "file_model.h"
#include "dom_item.h"

#include <QStyle>
#include <QApplication>
#include <QDebug>

FileModel::FileModel(QDomDocument document, QObject *parent):
    QAbstractItemModel(parent),
    _domDocument(document),
    _currentRow(0)
{
    _headers << "Name" << "Size" << "Last modified";

    QDomElement element = _domDocument.documentElement();
    QDomNode n = element.firstChild();
    _rootItem = new DomItem(n, 0);

    _folderIcon.addPixmap(QApplication::style()->standardPixmap(QStyle::SP_DirClosedIcon));
    _fileIcon.addPixmap(QApplication::style()->standardPixmap(QStyle::SP_FileIcon));
}

FileModel::~FileModel()
{
    delete _rootItem;
}

QVariant FileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return QVariant(_headers[section]);

    return QVariant();
}

QVariant FileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::DecorationRole && role != PathRole)
        return QVariant();

    DomItem *item = static_cast<DomItem*>(index.internalPointer());

    QDomNode node = item->node();
    QDomNamedNodeMap attributeMap = node.attributes();

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case 0:
            if (node.nodeName() == "file")
                return attributeMap.namedItem("name").nodeValue();
            else
                return attributeMap.namedItem("title").nodeValue();

        case 1:
            return attributeMap.namedItem("size").nodeValue();

        case 2:
            return attributeMap.item(2).nodeValue();

        default:
            return QVariant();
        }
    }
    else if (role == Qt::DecorationRole && index.column() == 0)
    {
        QDomNode attribute = attributeMap.item(0);
        if (attribute.nodeName() == "title")
            return _folderIcon;
        else if (attribute.nodeName() == "name")
            return _fileIcon;
    }
    else if (role == PathRole)
    {
        return attributeMap.namedItem("absolute").nodeValue();
    }

    return QVariant();

}

Qt::ItemFlags FileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex FileModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    DomItem *parentItem;

    if (!parent.isValid())
        parentItem = _rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());

    DomItem *childItem = parentItem->child(row);

    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex FileModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    DomItem *childItem = static_cast<DomItem*>(child.internalPointer());
    DomItem *parentItem = childItem->parent();

    if (!parentItem || parentItem == _rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int FileModel::columnCount(const QModelIndex &/*parent*/) const
{
    return _headers.count();
}

int FileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    DomItem *parentItem;

    if (!parent.isValid())
        parentItem = _rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());

    return parentItem->node().childNodes().count();
}

/*bool FileModel::canFetchMore(const QModelIndex &parent) const
{
    //if (_currentRow )
        DomItem *parentItem = static_cast<DomItem*>(parent.internalPointer());

    QDomNode node;
    if (parentItem)
        node = parentItem->node();
}*/

void FileModel::changeItems(const QModelIndex &parent, const QMap<QString, int> &dirContents)
{
    DomItem *parentItem;

    if (!parent.isValid())
        parentItem = _rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());

    parentItem->clearChildItems();

    int row = 0;
    QMapIterator<QString, int> it(dirContents);
    while(it.hasNext())
    {
        //parentItem->changeChildItems(row, dir);
        ++row;
    }


}
