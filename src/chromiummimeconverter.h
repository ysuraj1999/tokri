#ifndef CHROMIUMMIMECONVERTER_H
#define CHROMIUMMIMECONVERTER_H

#include <QtGlobal>

#ifdef Q_OS_WIN

#include <QWindowsMimeConverter>

class ChromiumMimeConverter : public QWindowsMimeConverter
{
public:
    ChromiumMimeConverter();

    bool canConvertToMime(const QString &mimeType, IDataObject *pDataObj) const override;
    QVariant convertToMime(const QString &mimeType, IDataObject *pDataObj,
                           QMetaType preferredType) const override;
    QString mimeForFormat(const FORMATETC &formatetc) const override;

    bool canConvertFromMime(const FORMATETC &formatetc,
                            const QMimeData *mimeData) const override;
    bool convertFromMime(const FORMATETC &formatetc, const QMimeData *mimeData,
                         STGMEDIUM *pmedium) const override;
    QList<FORMATETC> formatsForMime(const QString &mimeType,
                                    const QMimeData *mimeData) const override;

private:
    static bool hasIgnoreFormat(IDataObject *pDataObj);
};

#endif // Q_OS_WIN

#endif // CHROMIUMMIMECONVERTER_H
