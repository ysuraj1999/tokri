#ifndef TEXTFILE_H
#define TEXTFILE_H

#include <QFile>
#include <QImage>
#include <QMimeType>
#include <QObject>
#include <QSize>
#include <QString>

class TextFile : public QObject
{
    Q_OBJECT
public:
    explicit TextFile(QObject *parent = nullptr);
    void setName(QString name);
    void setContent(QString content);
    bool save();

    static bool isTextFile(const QString &path, const QMimeType *mime = nullptr);
    static QString readTextSnippet(const QString &path, int maxChars = 1000, int maxLines = 25);
    static QImage renderThumbnail(const QString &path, const QSize &targetSize, qreal dpr, bool isDark);

private:
    QString mName;
    QString mContents;
};

#endif // TEXTFILE_H
