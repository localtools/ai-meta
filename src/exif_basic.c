#include "exif_basic.h"

#include "util.h"

#include <stdlib.h>
#include <string.h>

/* Common EXIF tags of interest for AI tooling. */
enum {
    TAG_IMAGE_DESCRIPTION = 0x010E,
    TAG_MAKE = 0x010F,
    TAG_MODEL = 0x0110,
    TAG_SOFTWARE = 0x0131,
    TAG_ARTIST = 0x013B,
    TAG_USER_COMMENT = 0x9286,
    TAG_XPCOMMENT = 0x9C9C,
    TAG_EXIF_IFD = 0x8769
};

static uint16_t read16(const uint8_t *p, int le) {
    return le ? ai_meta_read_le16(p) : ai_meta_read_be16(p);
}

static uint32_t read32(const uint8_t *p, int le) {
    return le ? ai_meta_read_le32(p) : ai_meta_read_be32(p);
}

static char *read_ascii(const uint8_t *base, size_t len, uint32_t off, uint32_t count) {
    if (count == 0 || off >= len)
        return ai_meta_strdup("");
    size_t n = count;
    if (off + n > len)
        n = len - off;
    /* Trim trailing NUL */
    while (n > 0 && base[off + n - 1] == 0)
        n--;
    return ai_meta_strndup((const char *)(base + off), n);
}

static void parse_ifd(const uint8_t *base, size_t len, uint32_t ifd_off, int le,
                      ai_meta_info *info, int *likely, int depth) {
    if (depth > 3 || ifd_off + 2 > len)
        return;
    uint16_t entries = read16(base + ifd_off, le);
    if (ifd_off + 2u + (uint32_t)entries * 12u > len)
        return;

    for (uint16_t i = 0; i < entries; i++) {
        const uint8_t *e = base + ifd_off + 2 + (size_t)i * 12;
        uint16_t tag = read16(e, le);
        uint16_t type = read16(e + 2, le);
        uint32_t count = read32(e + 4, le);
        uint32_t valoff = read32(e + 8, le);
        const char *name = NULL;

        switch (tag) {
        case TAG_IMAGE_DESCRIPTION:
            name = "ImageDescription";
            break;
        case TAG_MAKE:
            name = "Make";
            break;
        case TAG_MODEL:
            name = "Model";
            break;
        case TAG_SOFTWARE:
            name = "Software";
            break;
        case TAG_ARTIST:
            name = "Artist";
            break;
        case TAG_USER_COMMENT:
            name = "UserComment";
            break;
        case TAG_EXIF_IFD:
            if (type == 4 && count == 1)
                parse_ifd(base, len, valoff, le, info, likely, depth + 1);
            continue;
        default:
            continue;
        }

        char *text = NULL;
        if (type == 2) { /* ASCII */
            if (count <= 4) {
                text = ai_meta_strndup((const char *)(e + 8), count);
                if (text) {
                    size_t n = strlen(text);
                    while (n > 0 && text[n - 1] == 0)
                        text[--n] = 0;
                }
            } else {
                text = read_ascii(base, len, valoff, count);
            }
        } else if (tag == TAG_USER_COMMENT && type == 7) {
            /* UNDEFINED: 8-byte charset code + comment */
            uint32_t off = count <= 4 ? (uint32_t)(e + 8 - base) : valoff;
            if (off + count <= len && count > 8) {
                text = ai_meta_strndup((const char *)(base + off + 8), count - 8);
            }
        }

        if (text && name) {
            if (ai_meta_value_looks_ai(text) || ai_meta_key_looks_ai(name))
                *likely = 1;
            if (info)
                (void)ai_meta_info_add_field(info, name, text, strlen(text), AI_META_SCHEME_EXIF);
            free(text);
        }
    }
}

static int parse_tiff(const uint8_t *tiff, size_t len, ai_meta_info *info, int *likely) {
    if (len < 8)
        return 0;
    int le = 0;
    if (tiff[0] == 'I' && tiff[1] == 'I')
        le = 1;
    else if (tiff[0] == 'M' && tiff[1] == 'M')
        le = 0;
    else
        return 0;
    uint16_t magic = read16(tiff + 2, le);
    if (magic != 42)
        return 0;
    uint32_t ifd0 = read32(tiff + 4, le);
    parse_ifd(tiff, len, ifd0, le, info, likely, 0);
    return 1;
}

ai_meta_err ai_meta_exif_extract(const uint8_t *exif, size_t len, ai_meta_info *info) {
    if (!exif || !info)
        return AI_META_ERR_INVALID_ARG;
    int likely = 0;
    const uint8_t *tiff = exif;
    size_t tlen = len;
    /* JPEG APP1 often prefixed with "Exif\0\0" */
    if (len >= 6 && memcmp(exif, "Exif\0\0", 6) == 0) {
        tiff = exif + 6;
        tlen = len - 6;
    }
    if (!parse_tiff(tiff, tlen, info, &likely))
        return AI_META_ERR_MALFORMED;
    if (likely) {
        info->likely_ai = 1;
        info->schemes |= AI_META_SCHEME_UNKNOWN_AI;
    }
    info->schemes |= AI_META_SCHEME_EXIF;
    return AI_META_OK;
}

int ai_meta_exif_has_ai_markers(const uint8_t *exif, size_t len) {
    ai_meta_info *tmp = NULL;
    if (ai_meta_info_create(&tmp) != AI_META_OK)
        return 0;
    int hit = 0;
    if (ai_meta_exif_extract(exif, len, tmp) == AI_META_OK)
        hit = tmp->likely_ai;
    ai_meta_info_free(tmp);
    return hit;
}
