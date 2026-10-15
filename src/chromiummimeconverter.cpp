#include "chromiummimeconverter.h"

#ifdef Q_OS_WIN

#include "chromiummimes.h"

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QVariant>

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <objidl.h>
#include <shlobj.h>

namespace {

CLIPFORMAT clipboardFormat(const wchar_t *name)
{
    return static_cast<CLIPFORMAT>(::RegisterClipboardFormatW(name));
}

CLIPFORMAT fileContentsFormat()
{
    static const CLIPFORMAT id = clipboardFormat(L"FileContents");
    return id;
}

CLIPFORMAT fileGroupDescriptorFormat()
{
    static const CLIPFORMAT id = clipboardFormat(L"FileGroupDescriptorW");
    return id;
}

CLIPFORMAT ignoreFormat()
{
    static const CLIPFORMAT id =
        clipboardFormat(L"chromium/x-ignore-file-contents");
    return id;
}

FORMATETC makeFormat(CLIPFORMAT cf, LONG lindex, DWORD tymed)
{
    FORMATETC fmt = {};
    fmt.cfFormat = cf;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = lindex;
    fmt.tymed = tymed;
    return fmt;
}

QByteArray readStream(IStream *stream)
{
    if (!stream)
        return {};

    const LARGE_INTEGER start = {};
    if (FAILED(stream->Seek(start, STREAM_SEEK_SET, nullptr)))
        return {};

    QByteArray out;
    char buffer[64 * 1024];
    for (;;) {
        ULONG read = 0;
        const HRESULT hr = stream->Read(buffer, static_cast<ULONG>(sizeof(buffer)), &read);
        if (read > 0)
            out.append(buffer, static_cast<int>(read));
        if (hr != S_OK || read == 0)
            break;
    }
    return out;
}

QByteArray readHGlobal(HGLOBAL global)
{
    if (!global)
        return {};

    const SIZE_T size = ::GlobalSize(global);
    const void *ptr = ::GlobalLock(global);
    if (!ptr)
        return {};

    const QByteArray out(static_cast<const char *>(ptr), static_cast<int>(size));
    ::GlobalUnlock(global);
    return out;
}

} // namespace

ChromiumMimeConverter::ChromiumMimeConverter() = default;

bool ChromiumMimeConverter::canConvertToMime(const QString &mimeType,
                                             IDataObject *pDataObj) const
{
    if (!pDataObj)
        return false;

    if (mimeType == QString::fromLatin1(TokriMime::FileContents)) {
        if (hasIgnoreFormat(pDataObj))
            return false;
        FORMATETC fmt = makeFormat(fileContentsFormat(), 0,
                                   TYMED_ISTREAM | TYMED_HGLOBAL);
        return pDataObj->QueryGetData(&fmt) == S_OK;
    }

    if (mimeType == QString::fromLatin1(TokriMime::FileName)) {
        FORMATETC fmt = makeFormat(fileGroupDescriptorFormat(), -1, TYMED_HGLOBAL);
        return pDataObj->QueryGetData(&fmt) == S_OK;
    }

    return false;
}

QVariant ChromiumMimeConverter::convertToMime(const QString &mimeType,
                                              IDataObject *pDataObj,
                                              QMetaType preferredType) const
{
    Q_UNUSED(preferredType);

    if (!pDataObj)
        return {};

    if (mimeType == QString::fromLatin1(TokriMime::FileContents)) {
        FORMATETC fmt = makeFormat(fileContentsFormat(), 0,
                                   TYMED_ISTREAM | TYMED_HGLOBAL);
        STGMEDIUM medium = {};
        if (pDataObj->GetData(&fmt, &medium) != S_OK)
            return {};

        QByteArray bytes;
        if (medium.tymed == TYMED_ISTREAM)
            bytes = readStream(medium.pstm);
        else if (medium.tymed == TYMED_HGLOBAL)
            bytes = readHGlobal(medium.hGlobal);

        ::ReleaseStgMedium(&medium);
        return bytes;
    }

    if (mimeType == QString::fromLatin1(TokriMime::FileName)) {
        FORMATETC fmt = makeFormat(fileGroupDescriptorFormat(), -1, TYMED_HGLOBAL);
        STGMEDIUM medium = {};
        if (pDataObj->GetData(&fmt, &medium) != S_OK)
            return {};

        QByteArray name;
        if (medium.tymed == TYMED_HGLOBAL && medium.hGlobal) {
            const auto *group = static_cast<const FILEGROUPDESCRIPTORW *>(
                ::GlobalLock(medium.hGlobal));
            if (group) {
                if (group->cItems > 0)
                    name = QString::fromWCharArray(group->fgd[0].cFileName).toUtf8();
                ::GlobalUnlock(medium.hGlobal);
            }
        }

        ::ReleaseStgMedium(&medium);
        return name;
    }

    return {};
}

QString ChromiumMimeConverter::mimeForFormat(const FORMATETC &formatetc) const
{
    if (formatetc.cfFormat == fileContentsFormat())
        return QString::fromLatin1(TokriMime::FileContents);
    if (formatetc.cfFormat == fileGroupDescriptorFormat())
        return QString::fromLatin1(TokriMime::FileName);
    return {};
}

bool ChromiumMimeConverter::canConvertFromMime(const FORMATETC &,
                                               const QMimeData *) const
{
    return false;
}

bool ChromiumMimeConverter::convertFromMime(const FORMATETC &, const QMimeData *,
                                            STGMEDIUM *) const
{
    return false;
}

QList<FORMATETC> ChromiumMimeConverter::formatsForMime(const QString &,
                                                       const QMimeData *) const
{
    return {};
}

bool ChromiumMimeConverter::hasIgnoreFormat(IDataObject *pDataObj)
{
    FORMATETC fmt = makeFormat(ignoreFormat(), -1, TYMED_HGLOBAL);
    return pDataObj->QueryGetData(&fmt) == S_OK;
}

#endif // Q_OS_WIN
