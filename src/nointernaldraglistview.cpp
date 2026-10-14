#include "nointernaldraglistview.h"

#include "loghelpers.h"
#include "themeprovider.h"

#include <QDragEnterEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainterPath>

namespace {

void preferCopy(QDropEvent *e)
{
    if (e->isAccepted()) {
        e->setDropAction(Qt::CopyAction);
        e->accept();
    }
}

}

NoInternalDragListView::NoInternalDragListView()
{
    connect(&mDragAnimationTimer, &QTimer::timeout, this, [this] {
        mDashOffset += 0.8;
        if (mDashOffset >= 10.0)
            mDashOffset -= 10.0;
        viewport()->update();
    });
}

void NoInternalDragListView::setDragHovering(bool hovering)
{
    if (mIsDragHovering == hovering)
        return;

    mIsDragHovering = hovering;
    if (mIsDragHovering) {
        mDashOffset = 0.0;
        mDragAnimationTimer.start(30); // ~33 FPS
    } else {
        mDragAnimationTimer.stop();
        mDashOffset = 0.0;
    }
    viewport()->update();
}

void NoInternalDragListView::startDrag(Qt::DropActions supportedActions)
{
    Logger &log = Logger::instance();
    log.push("startDrag");

    const Qt::KeyboardModifiers mods = QGuiApplication::keyboardModifiers();
#ifdef Q_OS_MACOS
    const bool forceCopy = mods.testFlag(Qt::AltModifier);
#else
    const bool forceCopy = mods.testFlag(Qt::ControlModifier);
#endif

    // If modifier is held, only allow copy (prevent moving).
    // Otherwise allow both Copy and Move so targets that only accept Copy
    // (such as web browsers like Firefox, Discord, text fields) or targets
    // that accept Move (such as file managers) can negotiate the drop.
    if (forceCopy) {
        supportedActions = Qt::CopyAction;
    } else {
        supportedActions = Qt::CopyAction | Qt::MoveAction;
    }

    // Default proposed action is CopyAction. This ensures that applications
    // that strictly reject MoveAction (e.g., Firefox, Chromium, web dropzones,
    // document viewers) will accept the drag. Targets that support MoveAction
    // can still choose to execute a Move.
    setDefaultDropAction(Qt::CopyAction);

    log.log() << "startDrag supportedActions=" << supportedActions
              << "defaultDropAction=" << defaultDropAction()
              << "forceCopy=" << forceCopy;

    QListView::startDrag(supportedActions);

    log.pop();
}

QItemSelectionModel::SelectionFlags
NoInternalDragListView::selectionCommand(const QModelIndex &index,
                                         const QEvent *event) const
{
    if (event && event->type() == QEvent::MouseButtonPress) {
        const auto *mouseEvent = static_cast<const QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::RightButton
            && selectionModel()
            && selectionModel()->hasSelection())
            return QItemSelectionModel::NoUpdate;
    }

    return QListView::selectionCommand(index, event);
}

void NoInternalDragListView::dragEnterEvent(QDragEnterEvent *e)
{
    const bool isSelf = e->source() == this;
    if (isSelf) {
        e->ignore();
    } else {
        setDragHovering(true);
        emit dropping(true);
        QListView::dragEnterEvent(e);
        preferCopy(e);
    }
}

void NoInternalDragListView::dragMoveEvent(QDragMoveEvent *e)
{
    const bool isSelf = e->source() == this;
    if (isSelf) {
        e->ignore();
    } else {
        if (!mIsDragHovering)
            setDragHovering(true);
        QListView::dragMoveEvent(e);
        preferCopy(e);
    }
}

void NoInternalDragListView::dragLeaveEvent(QDragLeaveEvent *e)
{
    Logger &log = Logger::instance();
    log.push("dragLeave");
    log.log() << "leave";

    setDragHovering(false);
    emit dropping(false);

    QListView::dragLeaveEvent(e);

    log.pop();
}

void NoInternalDragListView::dropEvent(QDropEvent *e)
{
    Logger &log = Logger::instance();
    log.push("dropEvent");

    log.log() << "source==this=" << (e->source() == this)
              << "dropAction=" << e->dropAction()
              << "possibleActions=" << e->possibleActions()
              << "formats=" << e->mimeData()->formats();

    setDragHovering(false);
    emit dropping(false);

    e->setDropAction(Qt::CopyAction);

    QListView::dropEvent(e);

    log.log() << "accepted=" << e->isAccepted()
              << "finalDropAction=" << e->dropAction();

    log.pop();
}

void NoInternalDragListView::emitVisibleCount()
{
    const QSize grid = gridSize();
    if (grid.isEmpty())
        return;

    const QSize vp = viewport()->size();
    const int cols = vp.width() / grid.width() + 1;
    const int rows = vp.height() / grid.height() + 1;
    const int count = cols * rows;

    if (count == mLastVisibleCount)
        return;

    mLastVisibleCount = count;
    emit visibleCountChanged(count);
}

void NoInternalDragListView::paintEvent(QPaintEvent *e) {
    emitVisibleCount();

    QListView::paintEvent(e);

    auto *m = model();
    const bool isEmpty = (!m || m->rowCount(rootIndex()) == 0);

    const QRectF dropRect = viewport()->rect().adjusted(16, 16, -16, -16);

    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);

    if (isEmpty) {
        if (mIsDragHovering) {
            drawDropFeedback(p, dropRect);
        }
        drawEmptyState(p);
    } else if (mIsDragHovering) {
        drawDropFeedback(p, dropRect);
    }
}

void NoInternalDragListView::drawDropFeedback(QPainter &p, const QRectF &rect)
{
    const bool isDark = ThemeProvider::isDark();

    // Soft bluish background fill
    const QColor bgHighlight = isDark ? QColor(24, 46, 78, 170)
                                      : QColor(230, 242, 255, 185);
    p.setPen(Qt::NoPen);
    p.setBrush(bgHighlight);
    p.drawRoundedRect(rect, 14.0, 14.0);

    // Animated dashed border
    const QColor borderColor = isDark ? QColor(82, 155, 255)
                                      : QColor(56, 132, 255);
    QPen dashPen(borderColor, 2.0, Qt::CustomDashLine, Qt::RoundCap, Qt::RoundJoin);
    dashPen.setDashPattern({5.0, 4.0});
    dashPen.setDashOffset(mDashOffset);

    p.setPen(dashPen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect, 14.0, 14.0);
}

void NoInternalDragListView::drawEmptyState(QPainter &p)
{
    const bool isDark = ThemeProvider::isDark();
    const qreal centerX = viewport()->width() / 2.0;

    // Document icon geometry (clean vector representation from Image 1)
    const qreal iconW = 46.0;
    const qreal iconH = 58.0;
    const qreal foldSize = 14.0;
    const qreal cornerRadius = 5.0;

    const QString line1 = QStringLiteral("Drop Your File(s)");
    const QString line2 = QStringLiteral("Here");

    QFont textFont = font();
    textFont.setPointSize(12);
    textFont.setWeight(QFont::Medium);
    const QFontMetrics fm(textFont);

    const int lineSpacing = fm.lineSpacing();
    const int totalTextH = lineSpacing * 2;
    const int gap = 16;
    const int totalContentH = int(iconH) + gap + totalTextH;

    const qreal startY = qMax(24.0, (viewport()->height() - totalContentH) / 2.0);
    const qreal iconLeft = centerX - iconW / 2.0;
    const qreal iconTop = startY;

    // Color based on theme and hover state
    QColor strokeColor;
    QColor textColor;
    if (mIsDragHovering) {
        strokeColor = isDark ? QColor(90, 162, 255) : QColor(43, 114, 235);
        textColor   = isDark ? QColor(90, 162, 255) : QColor(43, 114, 235);
    } else {
        strokeColor = isDark ? QColor(215, 212, 224) : QColor(48, 44, 56);
        textColor   = isDark ? QColor(170, 165, 180) : QColor(110, 105, 120);
    }

    QPen pen(strokeColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // Document body with top-right fold
    QPainterPath docPath;
    docPath.moveTo(iconLeft, iconTop + cornerRadius);
    docPath.quadTo(iconLeft, iconTop, iconLeft + cornerRadius, iconTop);
    docPath.lineTo(iconLeft + iconW - foldSize, iconTop);
    docPath.lineTo(iconLeft + iconW, iconTop + foldSize);
    docPath.lineTo(iconLeft + iconW, iconTop + iconH - cornerRadius);
    docPath.quadTo(iconLeft + iconW, iconTop + iconH, iconLeft + iconW - cornerRadius, iconTop + iconH);
    docPath.lineTo(iconLeft + cornerRadius, iconTop + iconH);
    docPath.quadTo(iconLeft, iconTop + iconH, iconLeft, iconTop + iconH - cornerRadius);
    docPath.closeSubpath();
    p.drawPath(docPath);

    // Fold flap
    QPainterPath foldPath;
    foldPath.moveTo(iconLeft + iconW - foldSize, iconTop);
    foldPath.lineTo(iconLeft + iconW - foldSize, iconTop + foldSize);
    foldPath.lineTo(iconLeft + iconW, iconTop + foldSize);
    p.drawPath(foldPath);

    // Upward arrow positioned in bottom-right of the document
    const qreal arrowX = iconLeft + iconW - 10.0;
    const qreal arrowBottom = iconTop + iconH + 3.0;
    const qreal arrowTop = iconTop + iconH - 22.0;

    // Arrow vertical shaft
    p.drawLine(QPointF(arrowX, arrowBottom), QPointF(arrowX, arrowTop));

    // Arrowhead (pointing up)
    QPainterPath headPath;
    headPath.moveTo(arrowX - 6.0, arrowTop + 7.0);
    headPath.lineTo(arrowX, arrowTop);
    headPath.lineTo(arrowX + 6.0, arrowTop + 7.0);
    p.drawPath(headPath);

    // Text: "Drop Your File(s)" / "Here"
    p.setFont(textFont);
    p.setPen(textColor);

    const qreal textTop = iconTop + iconH + gap;
    const QRectF line1Rect(16.0, textTop, viewport()->width() - 32.0, lineSpacing);
    const QRectF line2Rect(16.0, textTop + lineSpacing, viewport()->width() - 32.0, lineSpacing);

    p.drawText(line1Rect, Qt::AlignHCenter | Qt::AlignVCenter, line1);
    p.drawText(line2Rect, Qt::AlignHCenter | Qt::AlignVCenter, line2);
}
