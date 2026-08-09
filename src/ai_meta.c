#include "ai_meta.h"

#include "exif_basic.h"
#include "jpeg_meta.h"
#include "png_meta.h"
#include "util.h"
#include "webp_meta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *ai_meta_version(void) {
    return "0.1.0";
}

const char *ai_meta_strerror(ai_meta_err err) {
    switch (err) {
    case AI_META_OK:
        return "ok";
    case AI_META_ERR_INVALID_ARG:
        return "invalid argument";
    case AI_META_ERR_IO:
        return "I/O error";
    case AI_META_ERR_FORMAT:
        return "unrecognized or unsupported image format";
    case AI_META_ERR_NOMEM:
        return "out of memory";
    case AI_META_ERR_UNSUPPORTED:
        return "operation unsupported for this format/scheme";
    case AI_META_ERR_MALFORMED:
        return "malformed metadata (partial parse)";
    case AI_META_ERR_NOT_FOUND:
        return "not found";
    case AI_META_ERR_BUFFER_TOO_SMALL:
        return "buffer too small";
    default:
        return "unknown error";
    }
}

ai_meta_format ai_meta_detect_format(const uint8_t *data, size_t len) {
    if (ai_meta_is_png(data, len))
        return AI_META_FMT_PNG;
    if (ai_meta_is_jpeg(data, len))
        return AI_META_FMT_JPEG;
    if (ai_meta_is_webp(data, len))
        return AI_META_FMT_WEBP;
    return AI_META_FMT_UNKNOWN;
}

ai_meta_err ai_meta_scan(const uint8_t *data, size_t len, ai_meta_scan_result *out_result) {
    if (!data || !out_result || len == 0)
        return AI_META_ERR_INVALID_ARG;
    memset(out_result, 0, sizeof(*out_result));
    ai_meta_format fmt = ai_meta_detect_format(data, len);
    out_result->format = fmt;
    switch (fmt) {
    case AI_META_FMT_PNG:
        return ai_meta_png_scan(data, len, out_result);
    case AI_META_FMT_JPEG:
        return ai_meta_jpeg_scan(data, len, out_result);
    case AI_META_FMT_WEBP:
        return ai_meta_webp_scan(data, len, out_result);
    default:
        return AI_META_ERR_FORMAT;
    }
}

static void build_summary(ai_meta_info *info) {
    char buf[256];
    const char *fmt = "unknown";
    switch (info->format) {
    case AI_META_FMT_PNG:
        fmt = "PNG";
        break;
    case AI_META_FMT_JPEG:
        fmt = "JPEG";
        break;
    case AI_META_FMT_WEBP:
        fmt = "WebP";
        break;
    default:
        break;
    }
    snprintf(buf, sizeof(buf), "%s: schemes=0x%x fields=%zu likely_ai=%d", fmt, info->schemes,
             info->field_count, info->likely_ai);
    info->summary = ai_meta_strdup(buf);
}

ai_meta_err ai_meta_extract(const uint8_t *data, size_t len, ai_meta_info **out_info) {
    if (!data || !out_info || len == 0)
        return AI_META_ERR_INVALID_ARG;
    *out_info = NULL;
    ai_meta_info *info = NULL;
    ai_meta_err err = ai_meta_info_create(&info);
    if (err != AI_META_OK)
        return err;

    ai_meta_format fmt = ai_meta_detect_format(data, len);
    info->format = fmt;
    switch (fmt) {
    case AI_META_FMT_PNG:
        err = ai_meta_png_extract(data, len, info);
        break;
    case AI_META_FMT_JPEG:
        err = ai_meta_jpeg_extract(data, len, info);
        break;
    case AI_META_FMT_WEBP:
        err = ai_meta_webp_extract(data, len, info);
        break;
    default:
        ai_meta_info_free(info);
        return AI_META_ERR_FORMAT;
    }
    if (err != AI_META_OK && err != AI_META_ERR_MALFORMED) {
        ai_meta_info_free(info);
        return err;
    }
    build_summary(info);
    *out_info = info;
    return AI_META_OK;
}

ai_meta_err ai_meta_strip(const uint8_t *data, size_t len, unsigned flags, uint8_t **out_buf,
                          size_t *out_len) {
    if (!data || !out_buf || !out_len || len == 0)
        return AI_META_ERR_INVALID_ARG;
    if (flags == 0)
        flags = AI_META_FLAG_STRIP_ALL_AI | AI_META_FLAG_KEEP_COLOR_PROFILE;
    switch (ai_meta_detect_format(data, len)) {
    case AI_META_FMT_PNG:
        return ai_meta_png_strip(data, len, flags, out_buf, out_len);
    case AI_META_FMT_JPEG:
        return ai_meta_jpeg_strip(data, len, flags, out_buf, out_len);
    case AI_META_FMT_WEBP:
        return ai_meta_webp_strip(data, len, flags, out_buf, out_len);
    default:
        return AI_META_ERR_FORMAT;
    }
}

ai_meta_err ai_meta_write(const uint8_t *data, size_t len, const char *key, const char *value,
                          uint8_t **out_buf, size_t *out_len) {
    if (!data || !key || !out_buf || !out_len || len == 0)
        return AI_META_ERR_INVALID_ARG;
    switch (ai_meta_detect_format(data, len)) {
    case AI_META_FMT_PNG:
        return ai_meta_png_write(data, len, key, value, out_buf, out_len);
    case AI_META_FMT_JPEG:
        return ai_meta_jpeg_write(data, len, key, value, out_buf, out_len);
    case AI_META_FMT_WEBP:
        return ai_meta_webp_write(data, len, key, value, out_buf, out_len);
    default:
        return AI_META_ERR_FORMAT;
    }
}

ai_meta_err ai_meta_scan_file(const char *path, ai_meta_scan_result *out_result) {
    uint8_t *buf = NULL;
    size_t len = 0;
    ai_meta_err err = ai_meta_read_file(path, &buf, &len);
    if (err != AI_META_OK)
        return err;
    err = ai_meta_scan(buf, len, out_result);
    free(buf);
    return err;
}

ai_meta_err ai_meta_extract_file(const char *path, ai_meta_info **out_info) {
    uint8_t *buf = NULL;
    size_t len = 0;
    ai_meta_err err = ai_meta_read_file(path, &buf, &len);
    if (err != AI_META_OK)
        return err;
    err = ai_meta_extract(buf, len, out_info);
    free(buf);
    return err;
}

ai_meta_err ai_meta_strip_file(const char *path, const char *out_path, unsigned flags) {
    uint8_t *buf = NULL;
    size_t len = 0;
    ai_meta_err err = ai_meta_read_file(path, &buf, &len);
    if (err != AI_META_OK)
        return err;
    uint8_t *out = NULL;
    size_t out_len = 0;
    err = ai_meta_strip(buf, len, flags, &out, &out_len);
    free(buf);
    if (err != AI_META_OK)
        return err;
    err = ai_meta_write_file_bytes(out_path, out, out_len);
    free(out);
    return err;
}

ai_meta_err ai_meta_write_file(const char *path, const char *out_path, const char *key,
                               const char *value) {
    uint8_t *buf = NULL;
    size_t len = 0;
    ai_meta_err err = ai_meta_read_file(path, &buf, &len);
    if (err != AI_META_OK)
        return err;
    uint8_t *out = NULL;
    size_t out_len = 0;
    err = ai_meta_write(buf, len, key, value, &out, &out_len);
    free(buf);
    if (err != AI_META_OK)
        return err;
    err = ai_meta_write_file_bytes(out_path, out, out_len);
    free(out);
    return err;
}

void ai_meta_info_free(ai_meta_info *info) {
    if (!info)
        return;
    for (size_t i = 0; i < info->field_count; i++) {
        free(info->fields[i].key);
        free(info->fields[i].value);
    }
    free(info->fields);
    free(info->summary);
    free(info);
}

void ai_meta_buffer_free(uint8_t *buf) {
    free(buf);
}
