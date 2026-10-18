#ifndef THUMBNAILLAYOUT_H
#define THUMBNAILLAYOUT_H

#include <Qt>

namespace ThumbnailLayout {

inline constexpr int ThumbnailSize = 128;
inline constexpr int ColumnCount = 3;
inline constexpr int CaptionLineCount = 3;
inline constexpr int TileGap = 8;
inline constexpr int IconTopPadding = 4;
inline constexpr int IconCaptionGap = 6;
inline constexpr int CaptionBottomPadding = 4;

inline constexpr int TileWidth = ThumbnailSize + TileGap;

inline constexpr int MinViewportWidth = ColumnCount * TileWidth + 1;

inline constexpr int TileHeight(int fontHeight)
{
    return IconTopPadding + ThumbnailSize + IconCaptionGap
           + CaptionLineCount * fontHeight + CaptionBottomPadding + TileGap;
}

}

#endif // THUMBNAILLAYOUT_H
