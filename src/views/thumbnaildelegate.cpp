#include "views/thumbnaildelegate.h"

#include "views/thumbnaillayout.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QTextLayout>
#include <QTextOption>
#include <QWidget>

namespace {

void drawCaption(QPainter *painter, const QRect &cell, const QString &text,
                 const QFont &font, Qt::TextElideMode elideMode,
                 const QColor &color)
{
    using namespace ThumbnailLayout;

    const int top = cell.y() + IconTopPadding + ThumbnailSize + IconCaptionGap;
    const QRect textRect(cell.x(), top, cell.width(),
                         CaptionLineCount * QFontMetrics(font).height());
    if (!textRect.isValid() || text.isEmpty())
        return;

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setAlignment(Qt::AlignHCenter);

    QTextLayout layout(text, font);
    layout.setTextOption(textOption);

    const QFontMetrics fm(font);

    QVector<QTextLine> lines;
    QString elidedLast;
    layout.beginLayout();
    while (lines.size() < CaptionLineCount) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(textRect.width());
        lines.append(line);
    }
    // If the name does not fit, the whole remaining tail (start of the last
    // line through the end, including the extension) is middle-elided so the
    // file format stays visible.
    if (layout.createLine().isValid() && !lines.isEmpty())
        elidedLast = fm.elidedText(text.mid(lines.last().textStart()),
                                   elideMode, textRect.width());
    layout.endLayout();

    painter->save();
    painter->setPen(color);
    qreal y = textRect.top();
    for (int i = 0; i < lines.size(); ++i) {
        const bool isLast = (i == lines.size() - 1);
        const QString lineText =
            (isLast && !elidedLast.isEmpty())
                ? elidedLast
                : text.mid(lines[i].textStart(), lines[i].textLength());
        const int lineWidth = isLast && !elidedLast.isEmpty()
                                  ? fm.horizontalAdvance(lineText)
                                  : qRound(lines[i].naturalTextWidth());
        painter->drawText(
            QPointF(textRect.x() + (textRect.width() - lineWidth) / 2,
                    y + lines[i].ascent()),
            lineText);
        y += lines[i].height();
    }
    painter->restore();
}

}

void ThumbnailDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    using namespace ThumbnailLayout;

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const QIcon icon = opt.icon;
    const QString text = opt.text;

    opt.icon = QIcon();
    opt.text.clear();
    opt.features &= ~(QStyleOptionViewItem::HasDisplay
                      | QStyleOptionViewItem::HasDecoration);

    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QRect cell = opt.rect;

    if (!icon.isNull()) {
        const qreal dpr = opt.widget ? opt.widget->devicePixelRatioF()
                                     : qApp->devicePixelRatio();
        const QPixmap pm = icon.pixmap(QSize(ThumbnailSize, ThumbnailSize), dpr);
        if (!pm.isNull()) {
            const QSize size = pm.deviceIndependentSize().toSize();
            const QRect target(cell.x() + (cell.width() - ThumbnailSize) / 2,
                               cell.y() + IconTopPadding, ThumbnailSize,
                               ThumbnailSize);
            const QPoint topLeft(target.center().x() - size.width() / 2,
                                 target.center().y() - size.height() / 2);
            painter->drawPixmap(topLeft, pm);
        }
    }

    const QColor color =
        opt.palette.color((opt.state & QStyle::State_Selected)
                              ? QPalette::HighlightedText
                              : QPalette::Text);

    drawCaption(painter, cell, text, opt.font, opt.textElideMode, color);
}

QSize ThumbnailDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    Q_UNUSED(index);

    using namespace ThumbnailLayout;

    const QFontMetrics fm(option.font);
    return QSize(TileWidth, TileHeight(fm.height()));
}
