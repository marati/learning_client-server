#pragma once

#include <QStringList>
#include <QAbstractItemModel>
#include <QDomDocument>
#include <QModelIndex>
#include <QVariant>
#include <QIcon>

class DomItem;

class FileModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    FileModel(QDomDocument document, QObject *parent = 0);
    ~FileModel();

    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    //bool canFetchMore(const QModelIndex &parent) const;
    //void fetchMore(const QModelIndex &parent);

    void changeItems(const QModelIndex &parent, const QMap<QString, int> &dirContents);

    enum MyRoles
    {
        PathRole = Qt::UserRole + 1
    };

private:
    QStringList _headers;
    QDomDocument _domDocument;
    DomItem * _rootItem;
    QIcon _folderIcon;
    QIcon _fileIcon;
    int _currentRow;
};
