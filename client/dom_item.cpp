#include "dom_item.h"

#include <QtXml>

DomItem::DomItem(QDomNode &node, int row, DomItem *parent)
{
    _domNode = node;

    _rowNumber = row;
    _parentItem = parent;
}

DomItem::~DomItem()
{
    QHash<int, DomItem*>::iterator it;
    for (it = _childItems.begin(); it != _childItems.end(); ++it)
        delete it.value();
}

QDomNode DomItem::node() const
{
    return _domNode;
}

DomItem * DomItem::parent()
{
    return _parentItem;
}

DomItem * DomItem::child(int i)
{
    if (_childItems.contains(i))
        return _childItems[i];

    if (i >= 0 && i < _domNode.childNodes().count())
    {
        QDomNode childNode = _domNode.childNodes().item(i);

        DomItem *childItem = new DomItem(childNode, i, this);
        _childItems[i] = childItem;
        return childItem;
    }

    return 0;
}

void DomItem::clearChildItems()
{
    QHash<int, DomItem*>::iterator it;
    for (it = _childItems.begin(); it != _childItems.end(); ++it)
        delete it.value();
}

void DomItem::changeChildItems(int row, const QString &nodeType, const QString &value, int dirSize)
{
    QDomNode childNode = _domNode.childNodes().item(row);
    QDomNamedNodeMap attributeMap = childNode.attributes();

    if (childNode.nodeName() == "file")
    {
        attributeMap.namedItem("name").setNodeValue("kkh");
        //attributeMap.namedItem("name").set
    }

    childNode.setNodeValue(value);
    DomItem *childItem = new DomItem(childNode, row, this);
    _childItems[row] = childItem;
}

int DomItem::row()
{
    return _rowNumber;
}

int DomItem::rowCount()
{
    return _domNode.childNodes().count();
}
