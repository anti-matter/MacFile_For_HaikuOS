#ifndef __ClassicMacIcon_h__
#define __ClassicMacIcon_h__

#include <stdint.h>

/*
 * ClassicMacIcon
 *
 * A 32x32, 1-bit-per-pixel monochrome icon (128-byte bitmap + 128-byte
 * transparency mask = 256 bytes total) suitable for the AFP
 * "VolumeIconAndMask" field in the GetSrvrInfo response.
 *
 * The source artwork is the project's own 32x32 application icon
 * (BEOS:L:STD_ICON in afp_server.rsrc), thresholded to 1-bit.
 *
 * Bitmap: bit = 1 for black, 0 for white.  Mask: bit = 1 for
 * opaque (part of the icon), 0 for transparent.  Both are stored
 * row-major, MSB-first, top row first — the classic Mac layout.
 */

enum {
	kClassicVolumeIconBitmapSize	= 128,	/* 32 rows x 4 bytes */
	kClassicVolumeIconMaskSize		= 128,	/* 32 rows x 4 bytes */
	kClassicVolumeIconSize			= 256	/* bitmap + mask */
};

extern const uint8_t kClassicVolumeIconBitmap[kClassicVolumeIconBitmapSize];
extern const uint8_t kClassicVolumeIconMask[kClassicVolumeIconMaskSize];

#endif // __ClassicMacIcon_h__
