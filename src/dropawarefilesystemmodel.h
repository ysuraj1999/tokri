#ifndef DROPAWAREFILESYSTEMMODEL_H
#define DROPAWAREFILESYSTEMMODEL_H

#include <QDateTime>
#include <QFileSystemModel>
#include <QHash>
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
    struct ToolTipCacheEntry {
        QDateTime lastModified;
        qint64 size = 0;
        QString text;
    };
    mutable QHash<QString, ToolTipCacheEntry> mToolTipCache;
    QObject *mDropReceiver = nullptr;
};

#endif // DROPAWAREFILESYSTEMMODEL_H
