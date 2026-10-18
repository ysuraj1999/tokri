#ifndef DROPHANDLER_H
#define DROPHANDLER_H

#include <QImage>
#include <QMimeData>
#include <QObject>
#include <QUrl>

class DropHandler : public QObject
{
    Q_OBJECT
public:
    explicit DropHandler(QObject *parent = nullptr);

public slots:
    void handleDrop(QMimeData *data);

signals:
    void changed();
    void failed(const QString &reason);

private:
    bool tryMultiUrls(const QMimeData *data);
    bool tryLocalFile(const QMimeData *data);
    bool tryImage(const QMimeData *data);
    bool tryRemoteLink(const QMimeData *data);
    bool tryText(const QMimeData *data);
    bool tryHtml(const QMimeData *data);

    bool storeUrlItem(const QUrl &url);
    bool copyFile(const QString &filePath);
    bool copyDirectory(const QString &directory);
    bool saveFirstImage(const QMimeData *data);
    bool saveImageBytes(const QByteArray &bytes, const QString &mimeType);
    bool saveImage(const QImage &image);
    bool saveUrl(const QString &url);
    bool saveText(const QString &text);
    bool saveHtml(const QString &html);
};

#endif // DROPHANDLER_H
