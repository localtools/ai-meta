#ifndef AI_META_UTIL_H
#define AI_META_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ai_meta.h"

uint32_t ai_meta_crc32(const uint8_t *data, size_t len);
uint32_t ai_meta_read_be32(const uint8_t *p);
void ai_meta_write_be32(uint8_t *p, uint32_t v);
uint16_t ai_meta_read_be16(const uint8_t *p);
uint16_t ai_meta_read_le16(const uint8_t *p);
uint32_t ai_meta_read_le32(const uint8_t *p);
void ai_meta_write_le32(uint8_t *p, uint32_t v);

char *ai_meta_strdup(const char *s);
char *ai_meta_strndup(const char *s, size_t n);
int ai_meta_memmem_find(const uint8_t *hay, size_t hay_len, const uint8_t *needle,
                        size_t needle_len);

ai_meta_err ai_meta_read_file(const char *path, uint8_t **out, size_t *out_len);
ai_meta_err ai_meta_write_file_bytes(const char *path, const uint8_t *data, size_t len);

int ai_meta_key_looks_ai(const char *key);
int ai_meta_value_looks_ai(const char *value);
int ai_meta_xmp_looks_ai(const char *xmp, size_t len);

ai_meta_err ai_meta_info_create(ai_meta_info **out);
ai_meta_err ai_meta_info_add_field(ai_meta_info *info, const char *key, const char *value,
                                   size_t value_len, ai_meta_scheme scheme);

/* Expand Automatic1111 / SD-style "parameters" blobs into discrete fields. */
ai_meta_err ai_meta_parse_sd_parameters(ai_meta_info *info, const char *text, size_t text_len,
                                       ai_meta_scheme scheme);

/* Inflate zlib payload; caller frees *out with free(). Returns 0 on success. */
int ai_meta_zlib_inflate(const uint8_t *src, size_t src_len, uint8_t **out, size_t *out_len);

#endif
