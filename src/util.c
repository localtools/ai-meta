#include "util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint32_t crc_table[256];
static int crc_table_ready;

static void crc_init(void) {
    if (crc_table_ready)
        return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
    crc_table_ready = 1;
}

uint32_t ai_meta_crc32(const uint8_t *data, size_t len) {
    crc_init();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        c = crc_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

uint32_t ai_meta_read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

void ai_meta_write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

uint16_t ai_meta_read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

uint16_t ai_meta_read_le16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

uint32_t ai_meta_read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

void ai_meta_write_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

char *ai_meta_strdup(const char *s) {
    if (!s)
        return NULL;
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (!d)
        return NULL;
    memcpy(d, s, n + 1);
    return d;
}

char *ai_meta_strndup(const char *s, size_t n) {
    if (!s)
        return NULL;
    char *d = (char *)malloc(n + 1);
    if (!d)
        return NULL;
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

int ai_meta_memmem_find(const uint8_t *hay, size_t hay_len, const uint8_t *needle,
                        size_t needle_len) {
    if (!hay || !needle || needle_len == 0 || hay_len < needle_len)
        return -1;
    for (size_t i = 0; i + needle_len <= hay_len; i++) {
        if (memcmp(hay + i, needle, needle_len) == 0)
            return (int)i;
    }
    return -1;
}

ai_meta_err ai_meta_read_file(const char *path, uint8_t **out, size_t *out_len) {
    if (!path || !out || !out_len)
        return AI_META_ERR_INVALID_ARG;
    *out = NULL;
    *out_len = 0;
    FILE *f = fopen(path, "rb");
    if (!f)
        return AI_META_ERR_IO;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return AI_META_ERR_IO;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return AI_META_ERR_IO;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return AI_META_ERR_IO;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return AI_META_ERR_NOMEM;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(buf);
        return AI_META_ERR_IO;
    }
    *out = buf;
    *out_len = n;
    return AI_META_OK;
}

ai_meta_err ai_meta_write_file_bytes(const char *path, const uint8_t *data, size_t len) {
    if (!path || (!data && len) || (data == NULL && len == 0))
        return AI_META_ERR_INVALID_ARG;
    FILE *f = fopen(path, "wb");
    if (!f)
        return AI_META_ERR_IO;
    if (len && fwrite(data, 1, len, f) != len) {
        fclose(f);
        return AI_META_ERR_IO;
    }
    if (fclose(f) != 0)
        return AI_META_ERR_IO;
    return AI_META_OK;
}

static int contains_ci(const char *hay, const char *needle) {
    if (!hay || !needle)
        return 0;
    size_t nlen = strlen(needle);
    if (nlen == 0)
        return 1;
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == nlen)
            return 1;
    }
    return 0;
}

int ai_meta_key_looks_ai(const char *key) {
    if (!key)
        return 0;
    static const char *keys[] = {
        "parameters", "prompt", "negative prompt", "negative_prompt", "sd-metadata",
        "dream", "extras", "Comment", "Software", "UserComment", "ai", "generative",
        "invokeai", "comfyui", "automatic1111", "stable diffusion", NULL};
    for (int i = 0; keys[i]; i++) {
        if (contains_ci(key, keys[i]))
            return 1;
    }
    return 0;
}

int ai_meta_value_looks_ai(const char *value) {
    if (!value)
        return 0;
    static const char *markers[] = {
        "Steps:", "Sampler:", "CFG scale:", "Seed:", "Model hash:", "Model:",
        "Negative prompt:", "Clip skip:", "Denoising strength:", "Stable Diffusion",
        "DALL-E", "DALL·E", "Midjourney", "Firefly", "Craiyon", "NovelAI",
        "ComfyUI", "InvokeAI", "Automatic1111", "A1111", "SDXL", "Flux", NULL};
    for (int i = 0; markers[i]; i++) {
        if (contains_ci(value, markers[i]))
            return 1;
    }
    return 0;
}

int ai_meta_xmp_looks_ai(const char *xmp, size_t len) {
    if (!xmp || len == 0)
        return 0;
    /* Work on a temporary NUL-terminated copy for substring search. */
    char *tmp = ai_meta_strndup(xmp, len);
    if (!tmp)
        return 0;
    int hit = 0;
    static const char *markers[] = {
        "photoshop:Credit", "xmp:CreatorTool", "generative", "Firefly", "DALL",
        "Midjourney", "Stable Diffusion", "openai", "c2pa", "claim_generator",
        "DigitalSourceType", "trainedAlgorithmicMedia", "compositeWithTrainedAlgorithmicMedia",
        NULL};
    for (int i = 0; markers[i]; i++) {
        if (contains_ci(tmp, markers[i])) {
            hit = 1;
            break;
        }
    }
    free(tmp);
    return hit;
}

static int extract_xml_tag(const char *xml, const char *local_name, char *out, size_t out_sz) {
    /* Match <prefix:local>value</prefix:local> or <local>value</local> */
    char open1[96], open2[80];
    snprintf(open1, sizeof(open1), ":%s>", local_name);
    snprintf(open2, sizeof(open2), "<%s>", local_name);
    const char *p = strstr(xml, open1);
    if (p) {
        p += strlen(open1);
    } else {
        p = strstr(xml, open2);
        if (!p)
            return 0;
        p += strlen(open2);
    }
    const char *end = strchr(p, '<');
    if (!end || end <= p)
        return 0;
    size_t n = (size_t)(end - p);
    if (n >= out_sz)
        n = out_sz - 1;
    memcpy(out, p, n);
    out[n] = '\0';
    return n > 0;
}

ai_meta_err ai_meta_xmp_extract_fields(ai_meta_info *info, const char *xmp, size_t len) {
    if (!info || !xmp || len == 0)
        return AI_META_ERR_INVALID_ARG;
    char *tmp = ai_meta_strndup(xmp, len);
    if (!tmp)
        return AI_META_ERR_NOMEM;

    static const char *tags[] = {"CreatorTool", "DigitalSourceType", "Credit", "CreditLine",
                                 "claim_generator", "SoftwareAgent", "History", NULL};
    int added = 0;
    for (int i = 0; tags[i]; i++) {
        char val[512];
        if (extract_xml_tag(tmp, tags[i], val, sizeof(val))) {
            (void)ai_meta_info_add_field(info, tags[i], val, strlen(val), AI_META_SCHEME_XMP);
            added = 1;
        }
    }
    /* Custom ai_meta:* tags (skip names already pulled above). */
    const char *p = tmp;
    while ((p = strstr(p, "<ai_meta:")) != NULL) {
        p += 9;
        const char *gt = strchr(p, '>');
        if (!gt)
            break;
        size_t klen = (size_t)(gt - p);
        if (klen == 0 || klen > 64 || memchr(p, '/', klen)) {
            p = gt + 1;
            continue;
        }
        char key[65];
        memcpy(key, p, klen);
        key[klen] = '\0';
        int known = 0;
        for (int i = 0; tags[i]; i++) {
            if (strcmp(key, tags[i]) == 0) {
                known = 1;
                break;
            }
        }
        const char *vstart = gt + 1;
        const char *vend = strchr(vstart, '<');
        if (!vend) {
            p = vstart;
            continue;
        }
        if (!known) {
            (void)ai_meta_info_add_field(info, key, vstart, (size_t)(vend - vstart),
                                         AI_META_SCHEME_XMP);
            added = 1;
        }
        p = vend;
    }

    if (!added) {
        size_t preview = len > 240 ? 240 : len;
        (void)ai_meta_info_add_field(info, "XMP", tmp, preview, AI_META_SCHEME_XMP);
    } else {
        info->schemes |= AI_META_SCHEME_XMP;
        if (ai_meta_xmp_looks_ai(tmp, len)) {
            info->likely_ai = 1;
            info->schemes |= AI_META_SCHEME_UNKNOWN_AI;
        }
    }
    free(tmp);
    return AI_META_OK;
}

ai_meta_err ai_meta_info_create(ai_meta_info **out) {
    if (!out)
        return AI_META_ERR_INVALID_ARG;
    ai_meta_info *info = (ai_meta_info *)calloc(1, sizeof(*info));
    if (!info)
        return AI_META_ERR_NOMEM;
    *out = info;
    return AI_META_OK;
}

int ai_meta_zlib_inflate(const uint8_t *src, size_t src_len, uint8_t **out, size_t *out_len) {
    if (!src || !out || !out_len || src_len == 0)
        return -1;
    *out = NULL;
    *out_len = 0;
    size_t cap = src_len * 4 + 64;
    if (cap < 1024)
        cap = 1024;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf)
        return -1;

    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in = (Bytef *)src;
    strm.avail_in = (uInt)src_len;
    if (inflateInit(&strm) != Z_OK) {
        free(buf);
        return -1;
    }

    size_t total = 0;
    int ret;
    do {
        if (total + 256 > cap) {
            size_t ncap = cap * 2;
            uint8_t *nb = (uint8_t *)realloc(buf, ncap);
            if (!nb) {
                inflateEnd(&strm);
                free(buf);
                return -1;
            }
            buf = nb;
            cap = ncap;
        }
        strm.next_out = buf + total;
        strm.avail_out = (uInt)(cap - total);
        uInt before = strm.avail_out;
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            free(buf);
            return -1;
        }
        total += (size_t)(before - strm.avail_out);
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    *out = buf;
    *out_len = total;
    return 0;
}

static const char *find_ci(const char *hay, size_t hay_len, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0 || hay_len < nlen)
        return NULL;
    for (size_t i = 0; i + nlen <= hay_len; i++) {
        size_t j = 0;
        while (j < nlen &&
               tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j]))
            j++;
        if (j == nlen)
            return hay + i;
    }
    return NULL;
}

static void trim_range(const char *s, size_t n, const char **out, size_t *out_n) {
    while (n && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) {
        s++;
        n--;
    }
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        n--;
    *out = s;
    *out_n = n;
}

static ai_meta_err add_kv_slice(ai_meta_info *info, const char *key, const char *v, size_t vn,
                                ai_meta_scheme scheme) {
    const char *t;
    size_t tn;
    trim_range(v, vn, &t, &tn);
    if (tn == 0)
        return AI_META_OK;
    return ai_meta_info_add_field(info, key, t, tn, scheme);
}

ai_meta_err ai_meta_parse_sd_parameters(ai_meta_info *info, const char *text, size_t text_len,
                                       ai_meta_scheme scheme) {
    if (!info || !text)
        return AI_META_ERR_INVALID_ARG;

    const char *neg = find_ci(text, text_len, "Negative prompt:");
    const char *steps = find_ci(text, text_len, "\nSteps:");
    if (!steps)
        steps = find_ci(text, text_len, "Steps:");

    if (neg) {
        size_t prompt_len = (size_t)(neg - text);
        (void)add_kv_slice(info, "prompt", text, prompt_len, scheme);
        const char *neg_val = neg + strlen("Negative prompt:");
        size_t neg_end = text_len - (size_t)(neg_val - text);
        if (steps && steps > neg_val)
            neg_end = (size_t)(steps - neg_val);
        (void)add_kv_slice(info, "negative_prompt", neg_val, neg_end, scheme);
    } else if (steps && steps > text) {
        (void)add_kv_slice(info, "prompt", text, (size_t)(steps - text), scheme);
    }

    if (steps) {
        const char *p = steps;
        if (*p == '\n')
            p++;
        size_t rem = text_len - (size_t)(p - text);
        /* Walk "Key: value, Key: value" pairs */
        while (rem > 0) {
            const char *colon = memchr(p, ':', rem);
            if (!colon)
                break;
            size_t klen = (size_t)(colon - p);
            const char *k;
            size_t kn;
            trim_range(p, klen, &k, &kn);
            const char *vstart = colon + 1;
            size_t vrem = rem - (size_t)(vstart - p);
            const char *comma = memchr(vstart, ',', vrem);
            size_t vlen = comma ? (size_t)(comma - vstart) : vrem;
            if (kn > 0 && kn < 64) {
                char keybuf[65];
                memcpy(keybuf, k, kn);
                keybuf[kn] = '\0';
                (void)add_kv_slice(info, keybuf, vstart, vlen, scheme);
            }
            if (!comma)
                break;
            p = comma + 1;
            rem = text_len - (size_t)(p - text);
        }
    }
    return AI_META_OK;
}

ai_meta_err ai_meta_info_add_field(ai_meta_info *info, const char *key, const char *value,
                                   size_t value_len, ai_meta_scheme scheme) {
    if (!info || !key)
        return AI_META_ERR_INVALID_ARG;
    size_t ncap = info->field_count + 1;
    ai_meta_kv *nf =
        (ai_meta_kv *)realloc(info->fields, ncap * sizeof(ai_meta_kv));
    if (!nf)
        return AI_META_ERR_NOMEM;
    info->fields = nf;
    ai_meta_kv *kv = &info->fields[info->field_count];
    memset(kv, 0, sizeof(*kv));
    kv->key = ai_meta_strdup(key);
    if (!kv->key)
        return AI_META_ERR_NOMEM;
    if (value && value_len) {
        kv->value = ai_meta_strndup(value, value_len);
        if (!kv->value) {
            free(kv->key);
            kv->key = NULL;
            return AI_META_ERR_NOMEM;
        }
        kv->value_len = value_len;
    } else {
        kv->value = ai_meta_strdup("");
        if (!kv->value) {
            free(kv->key);
            kv->key = NULL;
            return AI_META_ERR_NOMEM;
        }
        kv->value_len = 0;
    }
    kv->scheme = scheme;
    info->field_count++;
    info->schemes |= (unsigned)scheme;
    if (ai_meta_key_looks_ai(key) || ai_meta_value_looks_ai(kv->value)) {
        info->likely_ai = 1;
        info->schemes |= AI_META_SCHEME_UNKNOWN_AI;
    }
    return AI_META_OK;
}
