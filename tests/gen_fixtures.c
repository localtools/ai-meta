/* Generate tiny PNG/JPEG/WebP fixtures with and without AI metadata. */
#include "ai_meta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static int write_bin(const char *path, const uint8_t *data, size_t len);

static uint32_t png_crc(const uint8_t *data, size_t len) {
    return (uint32_t)crc32(0L, data, (uInt)len);
}

static int write_ztxt_png(const char *path, const uint8_t *base, size_t base_len, const char *key,
                          const char *text) {
    /* Insert zTXt before IEND */
    if (base_len < 12)
        return -1;
    size_t iend_off = base_len - 12; /* IEND is last 12 bytes on our mini PNG */
    if (memcmp(base + iend_off + 4, "IEND", 4) != 0)
        return -1;

    uLongf bound = compressBound((uLong)strlen(text));
    uint8_t *comp = (uint8_t *)malloc(bound);
    if (!comp)
        return -1;
    uLongf clen = bound;
    if (compress(comp, &clen, (const Bytef *)text, (uLong)strlen(text)) != Z_OK) {
        free(comp);
        return -1;
    }
    size_t klen = strlen(key);
    size_t plen = klen + 1 + 1 + (size_t)clen; /* key\0 method + zlib */
    size_t chunk_total = 12 + plen;
    uint8_t *chunk = (uint8_t *)malloc(chunk_total);
    if (!chunk) {
        free(comp);
        return -1;
    }
    chunk[0] = (uint8_t)(plen >> 24);
    chunk[1] = (uint8_t)(plen >> 16);
    chunk[2] = (uint8_t)(plen >> 8);
    chunk[3] = (uint8_t)plen;
    memcpy(chunk + 4, "zTXt", 4);
    memcpy(chunk + 8, key, klen);
    chunk[8 + klen] = 0;
    chunk[9 + klen] = 0; /* zlib method */
    memcpy(chunk + 10 + klen, comp, clen);
    uint32_t crc = png_crc(chunk + 4, 4 + plen);
    chunk[8 + plen] = (uint8_t)(crc >> 24);
    chunk[9 + plen] = (uint8_t)(crc >> 16);
    chunk[10 + plen] = (uint8_t)(crc >> 8);
    chunk[11 + plen] = (uint8_t)crc;
    free(comp);

    size_t out_len = iend_off + chunk_total + 12;
    uint8_t *out = (uint8_t *)malloc(out_len);
    if (!out) {
        free(chunk);
        return -1;
    }
    memcpy(out, base, iend_off);
    memcpy(out + iend_off, chunk, chunk_total);
    memcpy(out + iend_off + chunk_total, base + iend_off, 12);
    free(chunk);
    int rc = write_bin(path, out, out_len);
    free(out);
    return rc;
}

/* Precomputed 1x1 RGB PNG (valid). */
static const uint8_t MINI_PNG[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90,
    0x77, 0x53, 0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8,
    0xCF, 0xC0, 0x00, 0x00, 0x03, 0x01, 0x01, 0x00, 0xF7, 0x03, 0x41, 0x43, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

/* Minimal JPEG: SOI + soft APP0 + EOI (enough for our segment walker). */
static const uint8_t MINI_JPEG[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x00, 0xFF, 0xD9};

/* Minimal WebP: RIFF + WEBP + VP8X (canvas 1x1) — no bitstream needed for meta walk. */
static const uint8_t MINI_WEBP[] = {
    'R', 'I', 'F', 'F', 0x1A, 0x00, 0x00, 0x00, 'W', 'E', 'B', 'P', 'V', 'P', '8', 'X', 0x0A,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static int write_bin(const char *path, const uint8_t *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static int path_join(char *dst, size_t dst_sz, const char *dir, const char *name) {
    int n = snprintf(dst, dst_sz, "%s/%s", dir, name);
    return (n < 0 || (size_t)n >= dst_sz) ? -1 : 0;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "tests/fixtures";
    char path[512];

    if (path_join(path, sizeof(path), dir, "clean.png") ||
        write_bin(path, MINI_PNG, sizeof(MINI_PNG))) {
        perror("clean.png");
        return 1;
    }

    uint8_t *out = NULL;
    size_t out_len = 0;
    const char *params =
        "masterpiece, best quality\nNegative prompt: lowres\nSteps: 20, Sampler: Euler a, "
        "CFG scale: 7, Seed: 42, Size: 512x512, Model: sd_v1.5";
    if (ai_meta_write(MINI_PNG, sizeof(MINI_PNG), "parameters", params, &out, &out_len) !=
        AI_META_OK) {
        fprintf(stderr, "png write failed\n");
        return 1;
    }
    if (path_join(path, sizeof(path), dir, "sd_parameters.png") || write_bin(path, out, out_len)) {
        perror("sd_parameters.png");
        ai_meta_buffer_free(out);
        return 1;
    }
    ai_meta_buffer_free(out);
    out = NULL;

    /* benign text */
    if (ai_meta_write(MINI_PNG, sizeof(MINI_PNG), "Title", "Holiday photo", &out, &out_len) !=
        AI_META_OK) {
        fprintf(stderr, "png title write failed\n");
        return 1;
    }
    if (path_join(path, sizeof(path), dir, "benign_text.png") || write_bin(path, out, out_len)) {
        perror("benign_text.png");
        ai_meta_buffer_free(out);
        return 1;
    }
    ai_meta_buffer_free(out);

    if (path_join(path, sizeof(path), dir, "clean.jpg") ||
        write_bin(path, MINI_JPEG, sizeof(MINI_JPEG))) {
        perror("clean.jpg");
        return 1;
    }

    /* JPEG with AI-looking COM */
    if (ai_meta_write(MINI_JPEG, sizeof(MINI_JPEG), "Software", "Stable Diffusion WebUI", &out,
                      &out_len) != AI_META_OK) {
        fprintf(stderr, "jpeg write failed\n");
        return 1;
    }
    if (path_join(path, sizeof(path), dir, "ai_com.jpg") || write_bin(path, out, out_len)) {
        perror("ai_com.jpg");
        ai_meta_buffer_free(out);
        return 1;
    }
    ai_meta_buffer_free(out);
    out = NULL;

    /* JPEG with Photoshop APP13 / IPTC Caption mentioning Firefly */
    {
        const char *caption = "Generated with Adobe Firefly";
        size_t cap_len = strlen(caption);
        size_t iptc_len = 5 + cap_len; /* 1C 02 78 len2 + data */
        /* 8BIM + id + name(empty=0 + pad) + size4 + iptc + pad */
        size_t irb_body = 4 + 2 + 2 + 4 + iptc_len + (iptc_len & 1);
        size_t app_payload = 14 + irb_body; /* Photoshop 3.0\0 + IRB */
        size_t seglen = 2 + app_payload;
        size_t jpeg_len = 2 + 2 + seglen + 2; /* SOI APP13 EOI */
        uint8_t *jpg = (uint8_t *)malloc(jpeg_len);
        if (!jpg)
            return 1;
        size_t w = 0;
        jpg[w++] = 0xFF;
        jpg[w++] = 0xD8;
        jpg[w++] = 0xFF;
        jpg[w++] = 0xED;
        jpg[w++] = (uint8_t)(seglen >> 8);
        jpg[w++] = (uint8_t)(seglen & 0xFF);
        memcpy(jpg + w, "Photoshop 3.0", 13);
        w += 13;
        jpg[w++] = 0;
        memcpy(jpg + w, "8BIM", 4);
        w += 4;
        jpg[w++] = 0x04;
        jpg[w++] = 0x04; /* IPTC-NAA */
        jpg[w++] = 0;     /* empty name */
        jpg[w++] = 0;     /* pad */
        jpg[w++] = (uint8_t)(iptc_len >> 24);
        jpg[w++] = (uint8_t)(iptc_len >> 16);
        jpg[w++] = (uint8_t)(iptc_len >> 8);
        jpg[w++] = (uint8_t)iptc_len;
        jpg[w++] = 0x1C;
        jpg[w++] = 2;
        jpg[w++] = 120; /* Caption */
        jpg[w++] = (uint8_t)(cap_len >> 8);
        jpg[w++] = (uint8_t)(cap_len & 0xFF);
        memcpy(jpg + w, caption, cap_len);
        w += cap_len;
        if (iptc_len & 1)
            jpg[w++] = 0;
        jpg[w++] = 0xFF;
        jpg[w++] = 0xD9;
        if (path_join(path, sizeof(path), dir, "iptc_ai.jpg") || write_bin(path, jpg, w)) {
            free(jpg);
            fprintf(stderr, "iptc_ai.jpg failed\n");
            return 1;
        }
        free(jpg);
    }

    if (path_join(path, sizeof(path), dir, "clean.webp") ||
        write_bin(path, MINI_WEBP, sizeof(MINI_WEBP))) {
        perror("clean.webp");
        return 1;
    }

    if (ai_meta_write(MINI_WEBP, sizeof(MINI_WEBP), "CreatorTool", "Adobe Firefly", &out,
                      &out_len) != AI_META_OK) {
        fprintf(stderr, "webp write failed\n");
        return 1;
    }
    if (path_join(path, sizeof(path), dir, "ai_xmp.webp") || write_bin(path, out, out_len)) {
        perror("ai_xmp.webp");
        ai_meta_buffer_free(out);
        return 1;
    }
    ai_meta_buffer_free(out);

    if (path_join(path, sizeof(path), dir, "sd_ztxt.png") ||
        write_ztxt_png(path, MINI_PNG, sizeof(MINI_PNG), "parameters",
                       "a cat\nNegative prompt: dog\nSteps: 15, Seed: 99, Model: demo")) {
        fprintf(stderr, "sd_ztxt.png failed\n");
        return 1;
    }

    /* Truncated / malformed PNG (signature + short IHDR) for graceful handling */
    static const uint8_t BAD_PNG[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00,
                                      0x00, 0x0D, 0x49, 0x48, 0x44, 0x52};
    if (path_join(path, sizeof(path), dir, "truncated.png") ||
        write_bin(path, BAD_PNG, sizeof(BAD_PNG))) {
        perror("truncated.png");
        return 1;
    }

    printf("fixtures written to %s\n", dir);
    return 0;
}
