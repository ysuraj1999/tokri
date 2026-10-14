#ifndef NOINTERNALDRAGLISTVIEW_H
#define NOINTERNALDRAGLISTVIEW_H

#include <QItemSelectionModel>
#include <QListView>
#include <QObject>
#include <QPainter>
#include <QTimer>

class NoInternalDragListView : public QListView
{
    Q_OBJECT
public:
    using QListView::QListView;

    NoInternalDragListView();

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    QItemSelectionModel::SelectionFlags selectionCommand(
        const QModelIndex &index, const QEvent *event = nullptr) const override;

signals:
    void dropping(bool status);
    void visibleCountChanged(int count);

private:
    void emitVisibleCount();
    void setDragHovering(bool hovering);
    void drawEmptyState(QPainter &p);
    void drawDropFeedback(QPainter &p, const QRectF &rect);

    int mLastVisibleCount = -1;
    bool mIsDragHovering = false;
    qreal mDashOffset = 0.0;
    QTimer mDragAnimationTimer;
};

#endif // NOINTERNALDRAGLISTVIEW_H
