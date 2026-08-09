#include "webp_meta.h"

#include "exif_basic.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ai_meta_is_webp(const uint8_t *data, size_t len) {
    return data && len >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0;
}

static ai_meta_err walk_chunks(const uint8_t *data, size_t len, ai_meta_scan_result *scan,
                               ai_meta_info *info, unsigned strip_flags, uint8_t **out_buf,
                               size_t *out_len, int do_strip) {
    if (len < 12)
        return AI_META_ERR_FORMAT;

    uint8_t *out = NULL;
    size_t w = 0;
    if (do_strip) {
        out = (uint8_t *)malloc(len);
        if (!out)
            return AI_META_ERR_NOMEM;
        memcpy(out, data, 12);
        w = 12;
    }

    size_t off = 12;
    while (off + 8 <= len) {
        const char *fourcc = (const char *)(data + off);
        uint32_t clen = ai_meta_read_le32(data + off + 4);
        size_t payload_off = off + 8;
        size_t padded = (size_t)clen + (clen & 1u);
        if (payload_off + padded > len)
            break;

        int drop = 0;
        int is_exif = memcmp(fourcc, "EXIF", 4) == 0;
        int is_xmp = memcmp(fourcc, "XMP ", 4) == 0;

        if (is_exif) {
            if (scan) {
                scan->schemes |= AI_META_SCHEME_EXIF;
                if (ai_meta_exif_has_ai_markers(data + payload_off, clen)) {
                    scan->likely_ai = 1;
                    scan->schemes |= AI_META_SCHEME_UNKNOWN_AI;
                }
            }
            if (info) {
                (void)ai_meta_exif_extract(data + payload_off, clen, info);
            }
            if (do_strip && (strip_flags & AI_META_FLAG_STRIP_EXIF))
                drop = 1;
        }
        if (is_xmp) {
            const char *xmp = (const char *)(data + payload_off);
            if (scan) {
                scan->schemes |= AI_META_SCHEME_XMP;
                if (ai_meta_xmp_looks_ai(xmp, clen)) {
                    scan->likely_ai = 1;
                    scan->schemes |= AI_META_SCHEME_UNKNOWN_AI;
                }
            }
            if (info) {
                info->schemes |= AI_META_SCHEME_XMP;
                (void)ai_meta_info_add_field(info, "XMP", xmp, clen, AI_META_SCHEME_XMP);
                if (ai_meta_xmp_looks_ai(xmp, clen)) {
                    info->likely_ai = 1;
                    info->schemes |= AI_META_SCHEME_UNKNOWN_AI;
                }
            }
            if (do_strip && (strip_flags & AI_META_FLAG_STRIP_XMP))
                drop = 1;
        }

        if (do_strip && !drop) {
            size_t chunk_total = 8 + padded;
            memcpy(out + w, data + off, chunk_total);
            w += chunk_total;
        }
        off = payload_off + padded;
    }

    if (do_strip) {
        /* Patch RIFF size */
        if (w >= 8)
            ai_meta_write_le32(out + 4, (uint32_t)(w - 8));
        *out_buf = out;
        *out_len = w;
    }
    return AI_META_OK;
}

ai_meta_err ai_meta_webp_scan(const uint8_t *data, size_t len, ai_meta_scan_result *out) {
    if (!ai_meta_is_webp(data, len) || !out)
        return AI_META_ERR_FORMAT;
    out->format = AI_META_FMT_WEBP;
    return walk_chunks(data, len, out, NULL, 0, NULL, NULL, 0);
}

ai_meta_err ai_meta_webp_extract(const uint8_t *data, size_t len, ai_meta_info *info) {
    if (!ai_meta_is_webp(data, len) || !info)
        return AI_META_ERR_FORMAT;
    info->format = AI_META_FMT_WEBP;
    return walk_chunks(data, len, NULL, info, 0, NULL, NULL, 0);
}

ai_meta_err ai_meta_webp_strip(const uint8_t *data, size_t len, unsigned flags,
                               uint8_t **out_buf, size_t *out_len) {
    if (!ai_meta_is_webp(data, len) || !out_buf || !out_len)
        return AI_META_ERR_FORMAT;
    return walk_chunks(data, len, NULL, NULL, flags, out_buf, out_len, 1);
}

ai_meta_err ai_meta_webp_write(const uint8_t *data, size_t len, const char *key,
                               const char *value, uint8_t **out_buf, size_t *out_len) {
    /*
     * Best-effort: append an XMP chunk with a simple RDF description of key/value.
     * Not a full XMP rewriter.
     */
    if (!ai_meta_is_webp(data, len) || !key || !out_buf || !out_len)
        return AI_META_ERR_INVALID_ARG;

    const char *val = value ? value : "";
    const char *fmt = "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>"
                      "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">"
                      "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
                      "<rdf:Description xmlns:ai_meta=\"https://example.com/ai_meta/1.0/\">"
                      "<ai_meta:%s>%s</ai_meta:%s>"
                      "</rdf:Description></rdf:RDF></x:xmpmeta><?xpacket end=\"w\"?>";
    int n = snprintf(NULL, 0, fmt, key, val, key);
    if (n < 0)
        return AI_META_ERR_NOMEM;
    char *xml = (char *)malloc((size_t)n + 1);
    if (!xml)
        return AI_META_ERR_NOMEM;
    if (snprintf(xml, (size_t)n + 1, fmt, key, val, key) != n) {
        free(xml);
        return AI_META_ERR_NOMEM;
    }

    /* Strip existing XMP then append */
    uint8_t *stripped = NULL;
    size_t stripped_len = 0;
    ai_meta_err err =
        ai_meta_webp_strip(data, len, AI_META_FLAG_STRIP_XMP, &stripped, &stripped_len);
    if (err != AI_META_OK) {
        free(xml);
        return err;
    }

    size_t xlen = strlen(xml);
    size_t pad = xlen & 1u;
    size_t total = stripped_len + 8 + xlen + pad;
    uint8_t *out = (uint8_t *)malloc(total);
    if (!out) {
        free(xml);
        free(stripped);
        return AI_META_ERR_NOMEM;
    }
    memcpy(out, stripped, stripped_len);
    size_t w = stripped_len;
    memcpy(out + w, "XMP ", 4);
    w += 4;
    ai_meta_write_le32(out + w, (uint32_t)xlen);
    w += 4;
    memcpy(out + w, xml, xlen);
    w += xlen;
    if (pad)
        out[w++] = 0;
    ai_meta_write_le32(out + 4, (uint32_t)(w - 8));
    free(xml);
    free(stripped);
    *out_buf = out;
    *out_len = w;
    return AI_META_OK;
}
