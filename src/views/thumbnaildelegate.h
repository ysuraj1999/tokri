#ifndef THUMBNAILDELGATE_H
#define THUMBNAILDELGATE_H

#include <QStyledItemDelegate>

class ThumbnailDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

#endif // THUMBNAILDELGATE_H
