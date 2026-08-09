#include "ai_meta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

static uint8_t *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t *)malloc((size_t)sz);
    if (!b) {
        fclose(f);
        return NULL;
    }
    *len = fread(b, 1, (size_t)sz, f);
    fclose(f);
    return b;
}

static void test_png_sd(void) {
    size_t len = 0;
    uint8_t *buf = slurp("tests/fixtures/sd_parameters.png", &len);
    expect(buf != NULL, "load sd_parameters.png");
    if (!buf)
        return;

    ai_meta_scan_result scan;
    expect(ai_meta_scan(buf, len, &scan) == AI_META_OK, "scan sd png");
    expect(scan.format == AI_META_FMT_PNG, "format png");
    expect((scan.schemes & AI_META_SCHEME_PNG_TEXT) != 0, "has png text");
    expect(scan.likely_ai == 1, "likely ai");

    ai_meta_info *info = NULL;
    expect(ai_meta_extract(buf, len, &info) == AI_META_OK, "extract sd png");
    expect(info && info->field_count >= 1, "has fields");
    int found = 0;
    if (info) {
        for (size_t i = 0; i < info->field_count; i++) {
            if (strcmp(info->fields[i].key, "parameters") == 0)
                found = 1;
        }
    }
    expect(found, "parameters key");
    int has_prompt = 0, has_steps = 0, has_seed = 0;
    if (info) {
        for (size_t i = 0; i < info->field_count; i++) {
            if (strcmp(info->fields[i].key, "prompt") == 0)
                has_prompt = 1;
            if (strcmp(info->fields[i].key, "Steps") == 0)
                has_steps = 1;
            if (strcmp(info->fields[i].key, "Seed") == 0)
                has_seed = 1;
        }
    }
    expect(has_prompt && has_steps && has_seed, "sd parameters expanded");

    uint8_t *stripped = NULL;
    size_t slen = 0;
    unsigned flags = AI_META_FLAG_STRIP_ALL_AI | AI_META_FLAG_KEEP_COLOR_PROFILE;
    expect(ai_meta_strip(buf, len, flags, &stripped, &slen) == AI_META_OK, "strip sd png");
    ai_meta_scan_result after;
    expect(ai_meta_scan(stripped, slen, &after) == AI_META_OK, "scan stripped");
    expect(after.likely_ai == 0, "stripped not likely ai");
    expect((after.schemes & AI_META_SCHEME_PNG_TEXT) == 0, "text removed");

    ai_meta_info_free(info);
    ai_meta_buffer_free(stripped);
    free(buf);
}

static void test_png_clean(void) {
    size_t len = 0;
    uint8_t *buf = slurp("tests/fixtures/clean.png", &len);
    expect(buf != NULL, "load clean.png");
    if (!buf)
        return;
    ai_meta_scan_result scan;
    expect(ai_meta_scan(buf, len, &scan) == AI_META_OK, "scan clean");
    expect(scan.likely_ai == 0, "clean not ai");
    expect(scan.schemes == 0, "no schemes");
    free(buf);
}

static void test_png_keep_non_ai(void) {
    size_t len = 0;
    uint8_t *buf = slurp("tests/fixtures/benign_text.png", &len);
    expect(buf != NULL, "load benign");
    if (!buf)
        return;
    uint8_t *out = NULL;
    size_t olen = 0;
    unsigned flags =
        AI_META_FLAG_STRIP_PNG_TEXT | AI_META_FLAG_KEEP_NON_AI_TEXT | AI_META_FLAG_KEEP_COLOR_PROFILE;
    expect(ai_meta_strip(buf, len, flags, &out, &olen) == AI_META_OK, "strip keep non-ai");
    ai_meta_info *info = NULL;
    expect(ai_meta_extract(out, olen, &info) == AI_META_OK, "extract after keep");
    int found = 0;
    if (info) {
        for (size_t i = 0; i < info->field_count; i++) {
            if (strcmp(info->fields[i].key, "Title") == 0)
                found = 1;
        }
    }
    expect(found, "Title preserved");
    ai_meta_info_free(info);
    ai_meta_buffer_free(out);
    free(buf);
}

static void test_png_ztxt(void) {
    size_t len = 0;
    uint8_t *buf = slurp("tests/fixtures/sd_ztxt.png", &len);
    expect(buf != NULL, "load sd_ztxt.png");
    if (!buf)
        return;
    ai_meta_info *info = NULL;
    expect(ai_meta_extract(buf, len, &info) == AI_META_OK, "extract ztxt");
    int has_seed = 0, has_prompt = 0;
    if (info) {
        for (size_t i = 0; i < info->field_count; i++) {
            if (strcmp(info->fields[i].key, "Seed") == 0 &&
                strcmp(info->fields[i].value, "99") == 0)
                has_seed = 1;
            if (strcmp(info->fields[i].key, "prompt") == 0)
                has_prompt = 1;
        }
    }
    expect(has_seed && has_prompt, "ztxt inflated + parsed");
    ai_meta_info_free(info);
    free(buf);
}

static void test_jpeg_com(void) {
    size_t len = 0;
    uint8_t *buf = slurp("tests/fixtures/ai_com.jpg", &len);
    expect(buf != NULL, "load ai_com.jpg");
    if (!buf)
        return;
    ai_meta_scan_result scan;
    expect(ai_meta_scan(buf, len, &scan) == AI_META_OK, "scan jpeg com");
    expect(scan.likely_ai == 1, "jpeg com likely ai");
    ai_meta_info *info = NULL;
    expect(ai_meta_extract(buf, len, &info) == AI_META_OK, "extract jpeg com");
    int found = 0;
    if (info) {
        for (size_t i = 0; i < info->field_count; i++) {
            if (strcmp(info->fields[i].key, "Software") == 0)
                found = 1;
        }
    }
    expect(found, "Software from COM");
    ai_meta_info_free(info);
    free(buf);
}

static void test_webp_xmp(void) {
    size_t len = 0;
    uint8_t *buf = slurp("tests/fixtures/ai_xmp.webp", &len);
    expect(buf != NULL, "load webp");
    if (!buf)
        return;
    ai_meta_scan_result scan;
    expect(ai_meta_scan(buf, len, &scan) == AI_META_OK, "scan webp");
    expect(scan.format == AI_META_FMT_WEBP, "webp fmt");
    expect((scan.schemes & AI_META_SCHEME_XMP) != 0, "has xmp");
    expect(scan.likely_ai == 1, "webp likely ai");
    ai_meta_info *info = NULL;
    expect(ai_meta_extract(buf, len, &info) == AI_META_OK, "extract webp xmp");
    int found = 0;
    if (info) {
        for (size_t i = 0; i < info->field_count; i++) {
            if (strcmp(info->fields[i].key, "CreatorTool") == 0 &&
                strstr(info->fields[i].value, "Firefly"))
                found = 1;
        }
    }
    expect(found, "CreatorTool from XMP");
    ai_meta_info_free(info);
    free(buf);
}

static void test_truncated(void) {
    size_t len = 0;
    uint8_t *buf = slurp("tests/fixtures/truncated.png", &len);
    expect(buf != NULL, "load truncated");
    if (!buf)
        return;
    ai_meta_scan_result scan;
    /* Should not crash; may return OK with partial or FORMAT */
    ai_meta_err err = ai_meta_scan(buf, len, &scan);
    expect(err == AI_META_OK || err == AI_META_ERR_FORMAT || err == AI_META_ERR_MALFORMED,
           "truncated handled");
    ai_meta_info *info = NULL;
    err = ai_meta_extract(buf, len, &info);
    expect(err == AI_META_OK || err == AI_META_ERR_FORMAT || err == AI_META_ERR_MALFORMED,
           "extract truncated handled");
    ai_meta_info_free(info);
    free(buf);
}

static void test_roundtrip_write(void) {
    size_t len = 0;
    uint8_t *buf = slurp("tests/fixtures/clean.png", &len);
    expect(buf != NULL, "load for write");
    if (!buf)
        return;
    uint8_t *out = NULL;
    size_t olen = 0;
    expect(ai_meta_write(buf, len, "parameters", "Steps: 10, Seed: 1", &out, &olen) == AI_META_OK,
           "write");
    ai_meta_scan_result scan;
    expect(ai_meta_scan(out, olen, &scan) == AI_META_OK, "scan written");
    expect(scan.likely_ai == 1, "written likely ai");
    ai_meta_buffer_free(out);
    free(buf);
}

int main(void) {
    printf("ai_meta tests (%s)\n", ai_meta_version());
    test_png_clean();
    test_png_sd();
    test_png_ztxt();
    test_png_keep_non_ai();
    test_jpeg_com();
    test_webp_xmp();
    test_truncated();
    test_roundtrip_write();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
