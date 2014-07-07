#pragma once

#include <QDomNode>
#include <QHash>

class DomItem
{
public:
    DomItem(QDomNode &node, int row, DomItem *parent = 0);
    ~DomItem();
    DomItem *child(int i);
    void clearChildItems();
    void changeChildItems(int row, const QString &nodeType, const QString &value, int dirSize);
    DomItem *parent();
    QDomNode node() const;
    int row();
    int rowCount();

private:
    QDomNode _domNode;
    QHash<int, DomItem*> _childItems;
    DomItem * _parentItem;
    int _rowNumber;
};
