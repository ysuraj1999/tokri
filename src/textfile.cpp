#include "textfile.h"

#include "loghelpers.h"

#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMimeDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QTextLayout>
#include <QTextOption>
#include <QTextStream>

TextFile::TextFile(QObject *parent)
    : QObject{parent}
{}

void TextFile::setName(QString name)
{
    mName = name;
}

void TextFile::setContent(QString content)
{
    mContents = content;
}

bool TextFile::save()
{
    Logger &log = Logger::instance();
    log.push("TextFile::save");

    if (mName.isEmpty()) {
        log.log() << "empty name -> false";
        log.pop();
        return false;
    }

    QFile file(mName);
    if (!file.open(QIODevice::NewOnly | QIODevice::WriteOnly | QIODevice::Text)) {
        log.log() << "open failed name=" << mName
                  << "err=" << file.errorString();
        log.pop();
        return false;
    }

    QTextStream out(&file);
    out << mContents;

    if (out.status() != QTextStream::Ok) {
        log.log() << "write failed name=" << mName
                  << "status=" << out.status();
        log.pop();
        return false;
    }

    log.log() << "name=" << mName
              << "chars=" << mContents.size() << "ok=true";

    log.pop();
    return true;
}

bool TextFile::isTextFile(const QString &path, const QMimeType *mime)
{
    const QFileInfo fi(path);
    const QString suffix = fi.suffix().toLower();

    static const QSet<QString> textExtensions = {
        QStringLiteral("txt"), QStringLiteral("text"), QStringLiteral("md"),
        QStringLiteral("markdown"), QStringLiteral("log"), QStringLiteral("csv"),
        QStringLiteral("json"), QStringLiteral("xml"), QStringLiteral("yaml"),
        QStringLiteral("yml"), QStringLiteral("ini"), QStringLiteral("conf"),
        QStringLiteral("cfg"), QStringLiteral("sh"), QStringLiteral("bat"),
        QStringLiteral("cmd"), QStringLiteral("py"), QStringLiteral("cpp"),
        QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("c"),
        QStringLiteral("js"), QStringLiteral("ts"), QStringLiteral("css"),
        QStringLiteral("html"), QStringLiteral("htm")
    };

    if (textExtensions.contains(suffix))
        return true;

    QMimeType fileMime;
    if (mime) {
        fileMime = *mime;
    } else {
        fileMime = QMimeDatabase().mimeTypeForFile(fi, QMimeDatabase::MatchContent);
    }

    if (fileMime.name().startsWith(QLatin1String("text/"))
        || fileMime.inherits(QLatin1String("text/plain"))) {
        return true;
    }

    return false;
}

QString TextFile::readTextSnippet(const QString &path, int maxChars, int maxLines)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QByteArray bytes = file.read(8192);
    if (bytes.contains('\0'))
        return {}; // Binary content

    QString text = QString::fromUtf8(bytes);
    if (text.isEmpty())
        return {};

    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    bool truncated = (file.size() > bytes.size());

    if (maxLines > 0) {
        const QStringList lines = text.split(QLatin1Char('\n'));
        if (lines.size() > maxLines) {
            text = lines.mid(0, maxLines).join(QLatin1Char('\n'));
            truncated = true;
        }
    }

    if (maxChars > 0 && text.size() > maxChars) {
        text = text.left(maxChars).trimmed();
        truncated = true;
    }

    if (truncated && !text.endsWith(QLatin1String("..."))) {
        text += QLatin1String("\n...");
    }

    return text;
}

QImage TextFile::renderThumbnail(const QString &path, const QSize &targetSize, qreal dpr, bool isDark)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QByteArray bytes = file.read(2048);
    if (bytes.contains('\0'))
        return {};

    QString rawText = QString::fromUtf8(bytes);
    rawText.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    rawText.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    rawText.replace(QLatin1Char('\t'), QLatin1String("    "));

    // Skip leading blank lines
    QStringList lines = rawText.split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.first().trimmed().isEmpty()) {
        lines.removeFirst();
    }
    const QString text = lines.join(QLatin1Char('\n')).trimmed();

    const QSize deviceSize = targetSize * dpr;
    QImage image(deviceSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    image.setDevicePixelRatio(dpr);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal canvasW = targetSize.width();
    const qreal canvasH = targetSize.height();

    // Document card geometry
    const qreal cardW = 104.0;
    const qreal cardH = 118.0;
    const qreal cardX = (canvasW - cardW) / 2.0;
    const qreal cardY = (canvasH - cardH) / 2.0;
    const qreal foldSize = 14.0;
    const qreal cornerRadius = 4.0;

    // Soft drop shadow
    const QColor shadowColor = isDark ? QColor(0, 0, 0, 80) : QColor(0, 0, 0, 22);
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadowColor);
    painter.drawRoundedRect(QRectF(cardX + 1.0, cardY + 2.5, cardW - 2.0, cardH - 1.0),
                            cornerRadius, cornerRadius);

    // Document page path with top-right dog-ear fold
    QPainterPath pagePath;
    pagePath.moveTo(cardX, cardY + cornerRadius);
    pagePath.quadTo(cardX, cardY, cardX + cornerRadius, cardY);
    pagePath.lineTo(cardX + cardW - foldSize, cardY);
    pagePath.lineTo(cardX + cardW, cardY + foldSize);
    pagePath.lineTo(cardX + cardW, cardY + cardH - cornerRadius);
    pagePath.quadTo(cardX + cardW, cardY + cardH, cardX + cardW - cornerRadius, cardY + cardH);
    pagePath.lineTo(cardX + cornerRadius, cardY + cardH);
    pagePath.quadTo(cardX, cardY + cardH, cardX, cardY + cardH - cornerRadius);
    pagePath.closeSubpath();

    // Theme colors
    const QColor pageBg = isDark ? QColor(36, 32, 42) : QColor(255, 255, 255);
    const QColor pageBorder = isDark ? QColor(68, 62, 78) : QColor(205, 200, 214);
    const QColor foldBg = isDark ? QColor(48, 43, 56) : QColor(230, 226, 238);
    const QColor foldBorder = isDark ? QColor(80, 73, 92) : QColor(190, 184, 202);
    const QColor textColor = isDark ? QColor(230, 228, 236) : QColor(28, 26, 32);
    const QColor badgeBg = isDark ? QColor(158, 108, 255, 40) : QColor(117, 55, 241, 24);
    const QColor badgeBorder = isDark ? QColor(158, 108, 255, 90) : QColor(117, 55, 241, 70);
    const QColor badgeText = isDark ? QColor(190, 158, 255) : QColor(117, 55, 241);

    // Draw document sheet
    painter.setPen(QPen(pageBorder, 1.0));
    painter.setBrush(pageBg);
    painter.drawPath(pagePath);

    // Draw dog-ear fold flap
    QPainterPath foldPath;
    foldPath.moveTo(cardX + cardW - foldSize, cardY);
    foldPath.lineTo(cardX + cardW - foldSize, cardY + foldSize);
    foldPath.lineTo(cardX + cardW, cardY + foldSize);
    foldPath.closeSubpath();

    painter.setPen(QPen(foldBorder, 1.0));
    painter.setBrush(foldBg);
    painter.drawPath(foldPath);

    // Draw file type badge (TXT or URL)
    const QFileInfo fi(path);
    const bool isLink = fi.fileName().startsWith(QLatin1String("link_"))
                        || text.startsWith(QLatin1String("http://"))
                        || text.startsWith(QLatin1String("https://"));
    const QString badgeStr = isLink ? QStringLiteral("URL") : QStringLiteral("TXT");

    const QRectF badgeRect(cardX + 8.0, cardY + 7.0, 24.0, 12.0);
    painter.setPen(QPen(badgeBorder, 0.8));
    painter.setBrush(badgeBg);
    painter.drawRoundedRect(badgeRect, 3.0, 3.0);

    QFont badgeFont = painter.font();
    badgeFont.setPointSize(6);
    badgeFont.setBold(true);
    painter.setFont(badgeFont);
    painter.setPen(badgeText);
    painter.drawText(badgeRect, Qt::AlignCenter, badgeStr);

    // Text content area
    const QRectF textArea(cardX + 8.0, cardY + 23.0, cardW - 16.0, cardH - 31.0);

    if (text.isEmpty()) {
        QFont emptyFont = painter.font();
        emptyFont.setPointSize(7);
        emptyFont.setItalic(true);
        painter.setFont(emptyFont);
        painter.setPen(isDark ? QColor(110, 105, 120) : QColor(160, 155, 170));
        painter.drawText(textArea, Qt::AlignLeft | Qt::AlignTop, QStringLiteral("(empty)"));
        return image;
    }

    QFont contentFont = painter.font();
    contentFont.setPointSize(7);
    contentFont.setBold(false);
    contentFont.setItalic(false);
    painter.setFont(contentFont);
    painter.setPen(textColor);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setAlignment(Qt::AlignLeft);

    QTextLayout layout(text, contentFont);
    layout.setTextOption(textOption);
    layout.beginLayout();

    qreal lineY = textArea.top();
    const int maxLines = 7;
    int lineCount = 0;
    bool hasOverflow = false;

    while (lineCount < maxLines) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(textArea.width());
        if (lineY + line.height() > textArea.bottom()) {
            hasOverflow = true;
            break;
        }
        line.draw(&painter, QPointF(textArea.left(), lineY));
        lineY += line.height();
        lineCount++;
    }
    if (layout.createLine().isValid()) {
        hasOverflow = true;
    }
    layout.endLayout();

    // If there is more text than fits on the card, smoothly fade out the bottom lines
    if (hasOverflow) {
        const qreal fadeHeight = 16.0;
        const QRectF fadeRect(textArea.left(), textArea.bottom() - fadeHeight,
                              textArea.width(), fadeHeight);
        QLinearGradient fade(0, fadeRect.top(), 0, fadeRect.bottom());
        fade.setColorAt(0.0, QColor(pageBg.red(), pageBg.green(), pageBg.blue(), 0));
        fade.setColorAt(1.0, pageBg);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fade);
        painter.drawRect(fadeRect);
    }

    return image;
}
