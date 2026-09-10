#ifndef __afpRefreshIcon_h__
#define __afpRefreshIcon_h__

#include <stdint.h>

/*
 * afpRefreshIcon
 *
 * A 16x16, 1-bit-per-pixel "refresh" (circular arrow) icon for the
 * Shared Folders list refresh button in the MacFile configuration app.
 *
 * Stored row-major, MSB-first, top row first — the same 1-bit layout as
 * the classic Mac volume icon in ClassicMacIcon.cpp.  A bit of 1 marks a
 * pixel of the icon (drawn in the button's label color); a bit of 0 is
 * transparent (the button background shows through).
 *
 * 16 rows x 2 bytes = 32 bytes.
 */

enum {
	kRefreshIconWidth	= 16,
	kRefreshIconHeight	= 16
};

extern const uint8_t kRefreshIconBitmap[kRefreshIconWidth * kRefreshIconHeight / 8];

// Returns true if the pixel at (x, y) is part of the icon.
static inline bool RefreshIconPixel(int32_t x, int32_t y)
{
	if (x < 0 || x >= kRefreshIconWidth || y < 0 || y >= kRefreshIconHeight)
		return false;

	int32_t index = y * kRefreshIconWidth + x;
	return (kRefreshIconBitmap[index / 8] >> (7 - (index % 8))) & 1;
}

#endif // __afpRefreshIcon_h__
