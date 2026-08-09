#include "png_meta.h"

#include "exif_basic.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static const uint8_t PNG_SIG[8] = {137, 80, 78, 71, 13, 10, 26, 10};

int ai_meta_is_png(const uint8_t *data, size_t len) {
    return data && len >= 8 && memcmp(data, PNG_SIG, 8) == 0;
}

static int chunk_is(const uint8_t *type, const char *name) {
    return memcmp(type, name, 4) == 0;
}

static int is_text_chunk(const uint8_t *type) {
    return chunk_is(type, "tEXt") || chunk_is(type, "iTXt") || chunk_is(type, "zTXt");
}

static int parse_text_keyword(const uint8_t *payload, size_t plen, char *key_out,
                              size_t key_cap, size_t *key_len, size_t *val_off) {
    size_t i = 0;
    while (i < plen && payload[i] != 0)
        i++;
    if (i == 0 || i >= plen || i >= key_cap)
        return 0;
    memcpy(key_out, payload, i);
    key_out[i] = '\0';
    *key_len = i;
    *val_off = i + 1;
    return 1;
}

static void consider_text(ai_meta_scan_result *out, const char *key, const char *val) {
    out->schemes |= AI_META_SCHEME_PNG_TEXT;
    if (ai_meta_key_looks_ai(key) || ai_meta_value_looks_ai(val)) {
        out->likely_ai = 1;
        out->schemes |= AI_META_SCHEME_UNKNOWN_AI;
    }
}

static int payload_has_c2pa(const uint8_t *payload, size_t clen) {
    if (!payload || clen < 4)
        return 0;
    if (ai_meta_memmem_find(payload, clen > 4096 ? 4096 : clen, (const uint8_t *)"c2pa", 4) >= 0)
        return 1;
    if (ai_meta_memmem_find(payload, clen > 4096 ? 4096 : clen, (const uint8_t *)"jumb", 4) >= 0)
        return 1;
    return 0;
}

/* Best-effort string pulls from C2PA JUMBF / CBOR-ish blobs in caBX. */
static void extract_c2pa_hints(ai_meta_info *info, const uint8_t *payload, size_t clen) {
    if (!info || !payload || clen == 0)
        return;
    info->schemes |= AI_META_SCHEME_C2PA;
    info->likely_ai = 1;
    info->schemes |= AI_META_SCHEME_UNKNOWN_AI;

    static const struct {
        const char *needle;
        const char *key;
    } hints[] = {{"OpenAI", "C2PA:generator"},
                 {"Google", "C2PA:generator"},
                 {"Adobe", "C2PA:generator"},
                 {"Firefly", "C2PA:generator"},
                 {"trainedAlgorithmicMedia", "C2PA:DigitalSourceType"},
                 {"compositeWithTrainedAlgorithmicMedia", "C2PA:DigitalSourceType"},
                 {NULL, NULL}};
    size_t scan_len = clen > 65536 ? 65536 : clen;
    for (int i = 0; hints[i].needle; i++) {
        size_t nlen = strlen(hints[i].needle);
        if (ai_meta_memmem_find(payload, scan_len, (const uint8_t *)hints[i].needle, nlen) >= 0)
            (void)ai_meta_info_add_field(info, hints[i].key, hints[i].needle, nlen,
                                         AI_META_SCHEME_C2PA);
    }
    (void)ai_meta_info_add_field(info, "C2PA", "[PNG caBX/JUMBF manifest present]", 33,
                                 AI_META_SCHEME_C2PA);
}

ai_meta_err ai_meta_png_scan(const uint8_t *data, size_t len, ai_meta_scan_result *out) {
    if (!ai_meta_is_png(data, len) || !out)
        return AI_META_ERR_FORMAT;
    out->format = AI_META_FMT_PNG;
    size_t off = 8;
    while (off + 12 <= len) {
        uint32_t clen = ai_meta_read_be32(data + off);
        const uint8_t *type = data + off + 4;
        size_t payload_off = off + 8;
        if (payload_off + (size_t)clen + 4 > len)
            break; /* truncated — stop gracefully */
        if (is_text_chunk(type) && clen > 0) {
            char key[80];
            size_t klen = 0, voff = 0;
            if (parse_text_keyword(data + payload_off, clen, key, sizeof(key), &klen, &voff)) {
                const char *val = (const char *)(data + payload_off + voff);
                size_t vlen = clen > voff ? clen - voff : 0;
                /* For iTXt/zTXt, value may start after compression flags; still scan raw. */
                char *tmp = ai_meta_strndup(val, vlen > 4096 ? 4096 : vlen);
                consider_text(out, key, tmp ? tmp : "");
                free(tmp);
            } else {
                out->schemes |= AI_META_SCHEME_PNG_TEXT;
            }
        }
        /* Heuristic: eXIf chunk */
        if (chunk_is(type, "eXIf"))
            out->schemes |= AI_META_SCHEME_EXIF;
        /* C2PA content credentials live in caBX (Content Authenticity Box / JUMBF). */
        if ((chunk_is(type, "caBX") || chunk_is(type, "c2pa")) &&
            payload_has_c2pa(data + payload_off, clen)) {
            out->schemes |= AI_META_SCHEME_C2PA;
            out->likely_ai = 1;
            out->schemes |= AI_META_SCHEME_UNKNOWN_AI;
        }
        off = payload_off + (size_t)clen + 4;
        if (chunk_is(type, "IEND"))
            break;
    }
    return AI_META_OK;
}

static ai_meta_err add_text_field(ai_meta_info *info, const uint8_t *type,
                                  const uint8_t *payload, size_t clen) {
    char key[128];
    size_t klen = 0, voff = 0;
    if (!parse_text_keyword(payload, clen, key, sizeof(key), &klen, &voff))
        return AI_META_OK; /* skip malformed */

    const uint8_t *rest = payload + voff;
    size_t rlen = clen - voff;
    const char *value = "";
    size_t value_len = 0;
    char *decoded = NULL;

    if (chunk_is(type, "tEXt")) {
        value = (const char *)rest;
        value_len = rlen;
    } else if (chunk_is(type, "zTXt")) {
        /* compression method (0 = zlib) then compressed text */
        if (rlen >= 2 && rest[0] == 0) {
            uint8_t *plain = NULL;
            size_t plain_len = 0;
            if (ai_meta_zlib_inflate(rest + 1, rlen - 1, &plain, &plain_len) == 0) {
                decoded = (char *)plain;
                value = decoded;
                value_len = plain_len;
            } else {
                decoded = ai_meta_strdup("[zTXt inflate failed]");
                value = decoded;
                value_len = decoded ? strlen(decoded) : 0;
            }
        }
    } else if (chunk_is(type, "iTXt")) {
        /* keyword\0 compression_flag compression_method language\0 translated\0 text */
        if (rlen >= 2) {
            uint8_t comp = rest[0];
            uint8_t method = rest[1];
            size_t p = 2; /* skip flag + method */
            while (p < rlen && rest[p] != 0)
                p++; /* language */
            if (p < rlen)
                p++;
            while (p < rlen && rest[p] != 0)
                p++; /* translated keyword */
            if (p < rlen)
                p++;
            if (comp && method == 0 && p < rlen) {
                uint8_t *plain = NULL;
                size_t plain_len = 0;
                if (ai_meta_zlib_inflate(rest + p, rlen - p, &plain, &plain_len) == 0) {
                    decoded = (char *)plain;
                    value = decoded;
                    value_len = plain_len;
                } else {
                    decoded = ai_meta_strdup("[iTXt inflate failed]");
                    value = decoded;
                    value_len = decoded ? strlen(decoded) : 0;
                }
            } else if (!comp) {
                value = (const char *)(rest + p);
                value_len = rlen > p ? rlen - p : 0;
            }
        }
    }

    ai_meta_err err =
        ai_meta_info_add_field(info, key, value, value_len, AI_META_SCHEME_PNG_TEXT);
    if (err == AI_META_OK && value && value_len &&
        (strcmp(key, "parameters") == 0 || strcmp(key, "Comment") == 0 ||
         strcmp(key, "prompt") == 0)) {
        char *nul = ai_meta_strndup(value, value_len);
        if (nul && ai_meta_value_looks_ai(nul))
            (void)ai_meta_parse_sd_parameters(info, nul, value_len, AI_META_SCHEME_PNG_TEXT);
        free(nul);
    }
    free(decoded);
    return err;
}

ai_meta_err ai_meta_png_extract(const uint8_t *data, size_t len, ai_meta_info *info) {
    if (!ai_meta_is_png(data, len) || !info)
        return AI_META_ERR_FORMAT;
    info->format = AI_META_FMT_PNG;
    size_t off = 8;
    while (off + 12 <= len) {
        uint32_t clen = ai_meta_read_be32(data + off);
        const uint8_t *type = data + off + 4;
        size_t payload_off = off + 8;
        if (payload_off + (size_t)clen + 4 > len)
            break;
        if (is_text_chunk(type)) {
            ai_meta_err err = add_text_field(info, type, data + payload_off, clen);
            if (err != AI_META_OK)
                return err;
        }
        if (chunk_is(type, "eXIf") && clen > 0) {
            (void)ai_meta_exif_extract(data + payload_off, clen, info);
        }
        if ((chunk_is(type, "caBX") || chunk_is(type, "c2pa")) &&
            payload_has_c2pa(data + payload_off, clen)) {
            extract_c2pa_hints(info, data + payload_off, clen);
        }
        off = payload_off + (size_t)clen + 4;
        if (chunk_is(type, "IEND"))
            break;
    }
    return AI_META_OK;
}

static int should_strip_text(unsigned flags, const char *key, const char *val_preview) {
    if (!(flags & AI_META_FLAG_STRIP_PNG_TEXT))
        return 0;
    if (flags & AI_META_FLAG_KEEP_NON_AI_TEXT) {
        if (!ai_meta_key_looks_ai(key) && !ai_meta_value_looks_ai(val_preview))
            return 0;
    }
    return 1;
}

ai_meta_err ai_meta_png_strip(const uint8_t *data, size_t len, unsigned flags,
                              uint8_t **out_buf, size_t *out_len) {
    if (!ai_meta_is_png(data, len) || !out_buf || !out_len)
        return AI_META_ERR_FORMAT;
    *out_buf = NULL;
    *out_len = 0;

    uint8_t *out = (uint8_t *)malloc(len);
    if (!out)
        return AI_META_ERR_NOMEM;
    memcpy(out, PNG_SIG, 8);
    size_t w = 8;
    size_t off = 8;

    while (off + 12 <= len) {
        uint32_t clen = ai_meta_read_be32(data + off);
        const uint8_t *type = data + off + 4;
        size_t chunk_total = 12u + (size_t)clen;
        if (off + chunk_total > len)
            break;

        int drop = 0;
        if (is_text_chunk(type)) {
            char key[128];
            size_t klen = 0, voff = 0;
            const char *preview = "";
            char *tmp = NULL;
            if (parse_text_keyword(data + off + 8, clen, key, sizeof(key), &klen, &voff)) {
                size_t plen = clen > voff ? clen - voff : 0;
                tmp = ai_meta_strndup((const char *)(data + off + 8 + voff),
                                      plen > 512 ? 512 : plen);
                preview = tmp ? tmp : "";
                drop = should_strip_text(flags, key, preview);
            } else if (flags & AI_META_FLAG_STRIP_PNG_TEXT) {
                drop = 1;
            }
            free(tmp);
        }
        if (chunk_is(type, "eXIf") && (flags & AI_META_FLAG_STRIP_EXIF))
            drop = 1;
        if ((chunk_is(type, "caBX") || chunk_is(type, "c2pa")) &&
            (flags & AI_META_FLAG_STRIP_C2PA))
            drop = 1;
        /* Never drop IHDR/IDAT/PLTE/IEND/iCCP when keeping color profile */
        if (chunk_is(type, "iCCP") && (flags & AI_META_FLAG_KEEP_COLOR_PROFILE))
            drop = 0;

        if (!drop) {
            memcpy(out + w, data + off, chunk_total);
            w += chunk_total;
        }
        off += chunk_total;
        if (chunk_is(type, "IEND"))
            break;
    }

    *out_buf = out;
    *out_len = w;
    return AI_META_OK;
}

static ai_meta_err build_text_chunk(const char *key, const char *value, uint8_t **chunk,
                                    size_t *chunk_len) {
    size_t klen = strlen(key);
    size_t vlen = value ? strlen(value) : 0;
    if (klen == 0 || klen > 79)
        return AI_META_ERR_INVALID_ARG;
    size_t plen = klen + 1 + vlen;
    size_t total = 12 + plen;
    uint8_t *c = (uint8_t *)malloc(total);
    if (!c)
        return AI_META_ERR_NOMEM;
    ai_meta_write_be32(c, (uint32_t)plen);
    memcpy(c + 4, "tEXt", 4);
    memcpy(c + 8, key, klen);
    c[8 + klen] = 0;
    if (vlen)
        memcpy(c + 9 + klen, value, vlen);
    uint32_t crc = ai_meta_crc32(c + 4, 4 + plen);
    ai_meta_write_be32(c + 8 + plen, crc);
    *chunk = c;
    *chunk_len = total;
    return AI_META_OK;
}

ai_meta_err ai_meta_png_write(const uint8_t *data, size_t len, const char *key,
                              const char *value, uint8_t **out_buf, size_t *out_len) {
    if (!ai_meta_is_png(data, len) || !key || !out_buf || !out_len)
        return AI_META_ERR_INVALID_ARG;

    uint8_t *new_chunk = NULL;
    size_t new_chunk_len = 0;
    ai_meta_err err = build_text_chunk(key, value ? value : "", &new_chunk, &new_chunk_len);
    if (err != AI_META_OK)
        return err;

    /* Replace existing tEXt with same keyword; insert before IEND otherwise. */
    uint8_t *out = (uint8_t *)malloc(len + new_chunk_len);
    if (!out) {
        free(new_chunk);
        return AI_META_ERR_NOMEM;
    }
    memcpy(out, PNG_SIG, 8);
    size_t w = 8;
    size_t off = 8;
    int replaced = 0;

    while (off + 12 <= len) {
        uint32_t clen = ai_meta_read_be32(data + off);
        const uint8_t *type = data + off + 4;
        size_t chunk_total = 12u + (size_t)clen;
        if (off + chunk_total > len)
            break;

        if (chunk_is(type, "IEND") && !replaced) {
            memcpy(out + w, new_chunk, new_chunk_len);
            w += new_chunk_len;
            memcpy(out + w, data + off, chunk_total);
            w += chunk_total;
            off += chunk_total;
            replaced = 1;
            break;
        }

        int skip = 0;
        if (chunk_is(type, "tEXt") || chunk_is(type, "iTXt") || chunk_is(type, "zTXt")) {
            char kbuf[128];
            size_t klen = 0, voff = 0;
            if (parse_text_keyword(data + off + 8, clen, kbuf, sizeof(kbuf), &klen, &voff) &&
                strcmp(kbuf, key) == 0) {
                memcpy(out + w, new_chunk, new_chunk_len);
                w += new_chunk_len;
                skip = 1;
                replaced = 1;
            }
        }
        if (!skip) {
            memcpy(out + w, data + off, chunk_total);
            w += chunk_total;
        }
        off += chunk_total;
        if (chunk_is(type, "IEND"))
            break;
    }

    free(new_chunk);
    if (!replaced) {
        free(out);
        return AI_META_ERR_MALFORMED;
    }
    *out_buf = out;
    *out_len = w;
    return AI_META_OK;
}
