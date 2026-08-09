/**
 * ai_meta — detect, extract, strip, and write AI-generation metadata in images.
 *
 * Supported schemes (MVP):
 *   - PNG tEXt / iTXt / zTXt (Stable Diffusion parameters, etc.)
 *   - Basic EXIF (Software, UserComment, ImageDescription, Artist)
 *   - Basic XMP (keyword / property scan for known AI markers)
 *
 * Stretch (stubbed API surface, not fully implemented):
 *   - C2PA manifests (requires signature/manifest stack)
 *   - IPTC (newsroom provenance)
 *
 * Formats: PNG, JPEG, WebP.
 *
 * Scope: declared metadata only — no pixel-based AI detection.
 *
 * License: MIT
 */
#ifndef AI_META_H
#define AI_META_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AI_META_VERSION_MAJOR 0
#define AI_META_VERSION_MINOR 1
#define AI_META_VERSION_PATCH 0

/* ---- Error codes (0 = success; negative = failure) ---- */
typedef enum ai_meta_err {
    AI_META_OK = 0,
    AI_META_ERR_INVALID_ARG = -1,
    AI_META_ERR_IO = -2,
    AI_META_ERR_FORMAT = -3,
    AI_META_ERR_NOMEM = -4,
    AI_META_ERR_UNSUPPORTED = -5,
    AI_META_ERR_MALFORMED = -6,
    AI_META_ERR_NOT_FOUND = -7,
    AI_META_ERR_BUFFER_TOO_SMALL = -8
} ai_meta_err;

/* ---- Detected metadata scheme bitmask ---- */
typedef enum ai_meta_scheme {
    AI_META_SCHEME_NONE = 0,
    AI_META_SCHEME_PNG_TEXT = 1u << 0, /* tEXt / iTXt / zTXt */
    AI_META_SCHEME_EXIF = 1u << 1,
    AI_META_SCHEME_XMP = 1u << 2,
    AI_META_SCHEME_IPTC = 1u << 3, /* JPEG APP13 Photoshop IRB / IPTC-IIM */
    AI_META_SCHEME_C2PA = 1u << 4, /* detect-only stub */
    AI_META_SCHEME_UNKNOWN_AI = 1u << 5 /* heuristic AI-looking keys */
} ai_meta_scheme;

typedef enum ai_meta_format {
    AI_META_FMT_UNKNOWN = 0,
    AI_META_FMT_PNG,
    AI_META_FMT_JPEG,
    AI_META_FMT_WEBP
} ai_meta_format;

/* Strip / write flags */
typedef enum ai_meta_flags {
    AI_META_FLAG_NONE = 0,
    AI_META_FLAG_STRIP_PNG_TEXT = 1u << 0,
    AI_META_FLAG_STRIP_EXIF = 1u << 1,
    AI_META_FLAG_STRIP_XMP = 1u << 2,
    AI_META_FLAG_STRIP_IPTC = 1u << 3,
    AI_META_FLAG_STRIP_C2PA = 1u << 4,
    /* Preserve ICC / color profile and other non-AI ancillary data when possible */
    AI_META_FLAG_KEEP_COLOR_PROFILE = 1u << 8,
    AI_META_FLAG_KEEP_NON_AI_TEXT = 1u << 9,
    AI_META_FLAG_STRIP_ALL_AI =
        (AI_META_FLAG_STRIP_PNG_TEXT | AI_META_FLAG_STRIP_EXIF |
         AI_META_FLAG_STRIP_XMP | AI_META_FLAG_STRIP_IPTC | AI_META_FLAG_STRIP_C2PA)
} ai_meta_flags;

typedef struct ai_meta_kv {
    char *key;
    char *value; /* UTF-8 when available; may be empty */
    size_t value_len;
    ai_meta_scheme scheme;
} ai_meta_kv;

typedef struct ai_meta_info {
    ai_meta_format format;
    unsigned schemes; /* ai_meta_scheme bitmask */
    int likely_ai;    /* 1 if known AI markers found */
    ai_meta_kv *fields;
    size_t field_count;
    char *summary; /* short human-readable summary; may be NULL */
} ai_meta_info;

typedef struct ai_meta_scan_result {
    ai_meta_format format;
    unsigned schemes;
    int likely_ai;
} ai_meta_scan_result;

/**
 * Memory model:
 *   - Primary API operates on caller-provided buffers (library does not own input).
 *   - Optional file helpers read/write paths for convenience.
 *   - All heap objects returned by the library must be freed with ai_meta_info_free()
 *     or ai_meta_buffer_free() as documented.
 *   - Strip/write produce a new buffer owned by the caller (free with ai_meta_buffer_free).
 */

const char *ai_meta_strerror(ai_meta_err err);
const char *ai_meta_version(void);

ai_meta_format ai_meta_detect_format(const uint8_t *data, size_t len);

/**
 * Scan buffer; fills out_result. Does not allocate field lists.
 */
ai_meta_err ai_meta_scan(const uint8_t *data, size_t len, ai_meta_scan_result *out_result);

/**
 * Extract all recognized AI-related (and optionally all text) fields.
 * On success, *out_info is heap-allocated; free with ai_meta_info_free().
 */
ai_meta_err ai_meta_extract(const uint8_t *data, size_t len, ai_meta_info **out_info);

/**
 * Strip schemes selected by flags. Writes a new image buffer.
 * out_buf/out_len owned by caller; free with ai_meta_buffer_free().
 * Color profiles (iCCP / ICC APP2) kept when AI_META_FLAG_KEEP_COLOR_PROFILE is set
 * (default recommended).
 */
ai_meta_err ai_meta_strip(const uint8_t *data, size_t len, unsigned flags,
                          uint8_t **out_buf, size_t *out_len);

/**
 * Inject or replace a key/value in the appropriate container for the format.
 * For PNG: writes/updates a tEXt (or iTXt if value needs UTF-8) chunk.
 * For JPEG/WebP: limited EXIF UserComment / XMP property update (best-effort).
 * Produces a new buffer; free with ai_meta_buffer_free().
 */
ai_meta_err ai_meta_write(const uint8_t *data, size_t len, const char *key,
                          const char *value, uint8_t **out_buf, size_t *out_len);

/* Optional file I/O helpers (library performs I/O). */
ai_meta_err ai_meta_scan_file(const char *path, ai_meta_scan_result *out_result);
ai_meta_err ai_meta_extract_file(const char *path, ai_meta_info **out_info);
ai_meta_err ai_meta_strip_file(const char *path, const char *out_path, unsigned flags);
ai_meta_err ai_meta_write_file(const char *path, const char *out_path, const char *key,
                               const char *value);

void ai_meta_info_free(ai_meta_info *info);
void ai_meta_buffer_free(uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* AI_META_H */
