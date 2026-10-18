#ifndef TEXTFILE_H
#define TEXTFILE_H

#include <QFile>
#include <QObject>

class TextFile : public QObject
{
    Q_OBJECT
public:
    explicit TextFile(QObject *parent = nullptr);
    void setName(QString name);
    void setContent(QString content);
    bool save();

private:
    QString mName;
    QString mContents;
};

#endif // TEXTFILE_H
