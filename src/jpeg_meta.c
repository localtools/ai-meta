#include "jpeg_meta.h"

#include "exif_basic.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

int ai_meta_is_jpeg(const uint8_t *data, size_t len) {
    return data && len >= 4 && data[0] == 0xFF && data[1] == 0xD8;
}

static int is_photoshop_app13(const uint8_t *seg, size_t seglen) {
    return seglen >= 14 && memcmp(seg, "Photoshop 3.0", 13) == 0 && seg[13] == 0;
}

/* Walk IPTC-IIM inside Photoshop IRB 8BIM resource 0x0404. */
static void iptc_from_irb(const uint8_t *payload, size_t plen, ai_meta_scan_result *scan,
                          ai_meta_info *info) {
    if (plen < 14 || !is_photoshop_app13(payload, plen))
        return;
    size_t off = 14;
    while (off + 12 <= plen) {
        if (memcmp(payload + off, "8BIM", 4) != 0)
            break;
        uint16_t res_id = ai_meta_read_be16(payload + off + 4);
        size_t name_len = payload[off + 6];
        size_t name_total = 1 + name_len + ((name_len + 1) & 1u); /* pad to even */
        size_t data_off = off + 4 + 2 + name_total;
        if (data_off + 4 > plen)
            break;
        uint32_t size = ai_meta_read_be32(payload + data_off);
        size_t data_start = data_off + 4;
        size_t data_end = data_start + size;
        if (data_end > plen)
            break;
        if (res_id == 0x0404) { /* IPTC-NAA */
            if (scan)
                scan->schemes |= AI_META_SCHEME_IPTC;
            size_t p = data_start;
            while (p + 5 <= data_end) {
                if (payload[p] != 0x1C) {
                    p++;
                    continue;
                }
                uint8_t rec = payload[p + 1];
                uint8_t ds = payload[p + 2];
                uint16_t dlen = ai_meta_read_be16(payload + p + 3);
                p += 5;
                if (p + dlen > data_end)
                    break;
                if (rec == 2 && dlen > 0) {
                    const char *name = NULL;
                    if (ds == 25)
                        name = "IPTC:Keywords";
                    else if (ds == 120)
                        name = "IPTC:Caption";
                    else if (ds == 105)
                        name = "IPTC:Headline";
                    else if (ds == 80)
                        name = "IPTC:Byline";
                    if (name || scan) {
                        char *tmp = ai_meta_strndup((const char *)(payload + p), dlen);
                        if (tmp) {
                            if (name && info)
                                (void)ai_meta_info_add_field(info, name, tmp, dlen,
                                                             AI_META_SCHEME_IPTC);
                            if (ai_meta_value_looks_ai(tmp)) {
                                if (scan) {
                                    scan->likely_ai = 1;
                                    scan->schemes |= AI_META_SCHEME_UNKNOWN_AI;
                                }
                                if (info) {
                                    info->likely_ai = 1;
                                    info->schemes |= AI_META_SCHEME_UNKNOWN_AI;
                                }
                            }
                            free(tmp);
                        }
                    }
                }
                p += dlen;
            }
            if (info)
                info->schemes |= AI_META_SCHEME_IPTC;
        }
        size_t padded = size + (size & 1u);
        off = data_start + padded;
    }
}

static int is_xmp_app1(const uint8_t *seg, size_t seglen) {
    /* Adobe XMP identifier is 29 bytes including trailing NUL. */
    if (seglen < 29)
        return 0;
    return memcmp(seg, "http://ns.adobe.com/xap/1.0/", 29) == 0;
}

static int is_exif_app1(const uint8_t *seg, size_t seglen) {
    return seglen >= 6 && memcmp(seg, "Exif\0\0", 6) == 0;
}

ai_meta_err ai_meta_jpeg_scan(const uint8_t *data, size_t len, ai_meta_scan_result *out) {
    if (!ai_meta_is_jpeg(data, len) || !out)
        return AI_META_ERR_FORMAT;
    out->format = AI_META_FMT_JPEG;
    size_t i = 2;
    while (i + 4 <= len) {
        if (data[i] != 0xFF)
            break;
        while (i < len && data[i] == 0xFF)
            i++;
        if (i >= len)
            break;
        uint8_t marker = data[i++];
        if (marker == 0xD9 || marker == 0xDA) /* EOI / SOS */
            break;
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7))
            continue;
        if (i + 2 > len)
            break;
        uint16_t seglen = ai_meta_read_be16(data + i);
        if (seglen < 2 || i + seglen > len)
            break;
        const uint8_t *payload = data + i + 2;
        size_t plen = (size_t)seglen - 2;
        if (marker == 0xE1) { /* APP1 */
            if (is_exif_app1(payload, plen)) {
                out->schemes |= AI_META_SCHEME_EXIF;
                if (ai_meta_exif_has_ai_markers(payload, plen)) {
                    out->likely_ai = 1;
                    out->schemes |= AI_META_SCHEME_UNKNOWN_AI;
                }
            } else if (is_xmp_app1(payload, plen)) {
                out->schemes |= AI_META_SCHEME_XMP;
                const char *xmp = (const char *)(payload + 29);
                size_t xlen = plen > 29 ? plen - 29 : 0;
                if (ai_meta_xmp_looks_ai(xmp, xlen)) {
                    out->likely_ai = 1;
                    out->schemes |= AI_META_SCHEME_UNKNOWN_AI;
                }
            }
        }
        /* C2PA JUMBF often in APP11 (0xEB) — mark presence only */
        if (marker == 0xEB && plen >= 4 &&
            ai_meta_memmem_find(payload, plen, (const uint8_t *)"c2pa", 4) >= 0) {
            out->schemes |= AI_META_SCHEME_C2PA;
        }
        if (marker == 0xFE && plen > 0) { /* COM */
            char *tmp = ai_meta_strndup((const char *)payload, plen);
            if (tmp && (ai_meta_value_looks_ai(tmp) || ai_meta_key_looks_ai(tmp))) {
                out->likely_ai = 1;
                out->schemes |= AI_META_SCHEME_UNKNOWN_AI;
            }
            free(tmp);
        }
        if (marker == 0xED) /* APP13 Photoshop IRB / IPTC */
            iptc_from_irb(payload, plen, out, NULL);
        i += seglen;
    }
    return AI_META_OK;
}

ai_meta_err ai_meta_jpeg_extract(const uint8_t *data, size_t len, ai_meta_info *info) {
    if (!ai_meta_is_jpeg(data, len) || !info)
        return AI_META_ERR_FORMAT;
    info->format = AI_META_FMT_JPEG;
    size_t i = 2;
    while (i + 4 <= len) {
        if (data[i] != 0xFF)
            break;
        while (i < len && data[i] == 0xFF)
            i++;
        if (i >= len)
            break;
        uint8_t marker = data[i++];
        if (marker == 0xD9 || marker == 0xDA)
            break;
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7))
            continue;
        if (i + 2 > len)
            break;
        uint16_t seglen = ai_meta_read_be16(data + i);
        if (seglen < 2 || i + seglen > len)
            break;
        const uint8_t *payload = data + i + 2;
        size_t plen = (size_t)seglen - 2;
        if (marker == 0xE1) {
            if (is_exif_app1(payload, plen)) {
                (void)ai_meta_exif_extract(payload, plen, info);
            } else if (is_xmp_app1(payload, plen)) {
                const char *xmp = (const char *)(payload + 29);
                size_t xlen = plen > 29 ? plen - 29 : 0;
                (void)ai_meta_xmp_extract_fields(info, xmp, xlen);
            }
        }
        if (marker == 0xEB && plen >= 4 &&
            ai_meta_memmem_find(payload, plen, (const uint8_t *)"c2pa", 4) >= 0) {
            info->schemes |= AI_META_SCHEME_C2PA;
            (void)ai_meta_info_add_field(info, "C2PA", "[JUMBF/C2PA segment present — parse stub]",
                                         42, AI_META_SCHEME_C2PA);
        }
        if (marker == 0xFE && plen > 0) {
            char *tmp = ai_meta_strndup((const char *)payload, plen);
            if (tmp) {
                /* COM often stores "key=value" from ai_meta_write */
                const char *eq = strchr(tmp, '=');
                if (eq && eq > tmp) {
                    char key[128];
                    size_t klen = (size_t)(eq - tmp);
                    if (klen >= sizeof(key))
                        klen = sizeof(key) - 1;
                    memcpy(key, tmp, klen);
                    key[klen] = '\0';
                    (void)ai_meta_info_add_field(info, key, eq + 1, strlen(eq + 1),
                                                 AI_META_SCHEME_UNKNOWN_AI);
                } else {
                    (void)ai_meta_info_add_field(info, "COM", tmp, strlen(tmp),
                                                 AI_META_SCHEME_UNKNOWN_AI);
                }
                if (ai_meta_value_looks_ai(tmp)) {
                    info->likely_ai = 1;
                    (void)ai_meta_parse_sd_parameters(info, tmp, strlen(tmp),
                                                     AI_META_SCHEME_UNKNOWN_AI);
                }
                free(tmp);
            }
        }
        if (marker == 0xED)
            iptc_from_irb(payload, plen, NULL, info);
        i += seglen;
    }
    return AI_META_OK;
}

ai_meta_err ai_meta_jpeg_strip(const uint8_t *data, size_t len, unsigned flags,
                               uint8_t **out_buf, size_t *out_len) {
    if (!ai_meta_is_jpeg(data, len) || !out_buf || !out_len)
        return AI_META_ERR_FORMAT;
    uint8_t *out = (uint8_t *)malloc(len);
    if (!out)
        return AI_META_ERR_NOMEM;
    out[0] = 0xFF;
    out[1] = 0xD8;
    size_t w = 2;
    size_t i = 2;

    while (i < len) {
        if (data[i] != 0xFF) {
            /* Copy rest (entropy-coded data after SOS already handled by break). */
            memcpy(out + w, data + i, len - i);
            w += len - i;
            break;
        }
        size_t mark_start = i;
        while (i < len && data[i] == 0xFF)
            i++;
        if (i >= len)
            break;
        uint8_t marker = data[i++];
        if (marker == 0xD9) {
            out[w++] = 0xFF;
            out[w++] = 0xD9;
            break;
        }
        if (marker == 0xDA) {
            /* Copy SOS + remainder */
            memcpy(out + w, data + mark_start, len - mark_start);
            w += len - mark_start;
            break;
        }
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            out[w++] = 0xFF;
            out[w++] = marker;
            continue;
        }
        if (i + 2 > len)
            break;
        uint16_t seglen = ai_meta_read_be16(data + i);
        if (seglen < 2 || i + seglen > len)
            break;
        const uint8_t *payload = data + i + 2;
        size_t plen = (size_t)seglen - 2;
        int drop = 0;
        if (marker == 0xE1) {
            if (is_exif_app1(payload, plen) && (flags & AI_META_FLAG_STRIP_EXIF))
                drop = 1;
            if (is_xmp_app1(payload, plen) && (flags & AI_META_FLAG_STRIP_XMP))
                drop = 1;
        }
        if (marker == 0xEB && (flags & AI_META_FLAG_STRIP_C2PA) &&
            ai_meta_memmem_find(payload, plen, (const uint8_t *)"c2pa", 4) >= 0)
            drop = 1;
        if (marker == 0xFE && plen > 0 && (flags & AI_META_FLAG_STRIP_ALL_AI)) {
            char *tmp = ai_meta_strndup((const char *)payload, plen);
            if (tmp && ai_meta_value_looks_ai(tmp))
                drop = 1;
            free(tmp);
        }
        if (marker == 0xED && (flags & AI_META_FLAG_STRIP_IPTC) &&
            is_photoshop_app13(payload, plen))
            drop = 1;
        /* APP2 ICC kept when KEEP_COLOR_PROFILE */
        if (marker == 0xE2 && (flags & AI_META_FLAG_KEEP_COLOR_PROFILE))
            drop = 0;

        if (!drop) {
            size_t chunk = (size_t)((i + seglen) - mark_start);
            memcpy(out + w, data + mark_start, chunk);
            w += chunk;
        }
        i += seglen;
    }

    *out_buf = out;
    *out_len = w;
    return AI_META_OK;
}

ai_meta_err ai_meta_jpeg_write(const uint8_t *data, size_t len, const char *key,
                               const char *value, uint8_t **out_buf, size_t *out_len) {
    /*
     * Best-effort: inject a COM marker (0xFE) with "key=value".
     * Full EXIF rewrite is out of scope without libexif.
     */
    if (!ai_meta_is_jpeg(data, len) || !key || !out_buf || !out_len)
        return AI_META_ERR_INVALID_ARG;
    const char *val = value ? value : "";
    size_t klen = strlen(key);
    size_t vlen = strlen(val);
    size_t text_len = klen + 1 + vlen;
    size_t seglen = 2 + text_len;
    if (seglen > 0xFFFF)
        return AI_META_ERR_INVALID_ARG;

    size_t new_len = len + 2 + seglen;
    uint8_t *out = (uint8_t *)malloc(new_len);
    if (!out)
        return AI_META_ERR_NOMEM;
    size_t w = 0;
    out[w++] = data[0];
    out[w++] = data[1];
    out[w++] = 0xFF;
    out[w++] = 0xFE;
    out[w++] = (uint8_t)(seglen >> 8);
    out[w++] = (uint8_t)(seglen & 0xFF);
    memcpy(out + w, key, klen);
    w += klen;
    out[w++] = '=';
    memcpy(out + w, val, vlen);
    w += vlen;
    memcpy(out + w, data + 2, len - 2);
    w += len - 2;
    *out_buf = out;
    *out_len = w;
    return AI_META_OK;
}
