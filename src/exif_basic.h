#ifndef AI_META_EXIF_BASIC_H
#define AI_META_EXIF_BASIC_H

#include "ai_meta.h"

/* Parse TIFF-based EXIF blob (byte-order II or MM). Adds interesting tags to info. */
ai_meta_err ai_meta_exif_extract(const uint8_t *exif, size_t len, ai_meta_info *info);
int ai_meta_exif_has_ai_markers(const uint8_t *exif, size_t len);

#endif
