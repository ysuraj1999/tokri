#ifndef DROPAWAREFILESYSTEMMODEL_H
#define DROPAWAREFILESYSTEMMODEL_H

#include <QFileSystemModel>
#include <QMimeData>

class DropAwareFileSystemModel : public QFileSystemModel
{
    Q_OBJECT
public:
    explicit DropAwareFileSystemModel(QObject *parent = nullptr);

    void setDropReceiver(QObject *receiver);

    static bool isPasteable(const QMimeData *data);

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool canDropMimeData(const QMimeData *data,
                         Qt::DropAction action,
                         int row, int column,
                         const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data,
                      Qt::DropAction action,
                      int row, int column,
                      const QModelIndex &parent) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QMimeData* mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;

signals:
    void dropReceived(QMimeData *data);

private:
    QObject *mDropReceiver = nullptr;
};

#endif // DROPAWAREFILESYSTEMMODEL_H
