#ifndef AI_META_WEBP_H
#define AI_META_WEBP_H

#include "ai_meta.h"

int ai_meta_is_webp(const uint8_t *data, size_t len);
ai_meta_err ai_meta_webp_scan(const uint8_t *data, size_t len, ai_meta_scan_result *out);
ai_meta_err ai_meta_webp_extract(const uint8_t *data, size_t len, ai_meta_info *info);
ai_meta_err ai_meta_webp_strip(const uint8_t *data, size_t len, unsigned flags,
                               uint8_t **out_buf, size_t *out_len);
ai_meta_err ai_meta_webp_write(const uint8_t *data, size_t len, const char *key,
                               const char *value, uint8_t **out_buf, size_t *out_len);

#endif
