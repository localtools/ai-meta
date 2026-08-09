#include "ai_meta.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *argv0) {
    fprintf(stderr,
            "Usage:\n"
            "  %s scan <file>\n"
            "  %s extract <file>\n"
            "  %s strip <in> <out> [--keep-non-ai]\n"
            "  %s write <in> <out> <key> <value>\n",
            argv0, argv0, argv0, argv0);
}

static const char *fmt_name(ai_meta_format f) {
    switch (f) {
    case AI_META_FMT_PNG:
        return "PNG";
    case AI_META_FMT_JPEG:
        return "JPEG";
    case AI_META_FMT_WEBP:
        return "WebP";
    default:
        return "unknown";
    }
}

static void print_schemes(unsigned s) {
    if (s == 0) {
        printf("  (none)\n");
        return;
    }
    if (s & AI_META_SCHEME_PNG_TEXT)
        printf("  PNG_TEXT\n");
    if (s & AI_META_SCHEME_EXIF)
        printf("  EXIF\n");
    if (s & AI_META_SCHEME_XMP)
        printf("  XMP\n");
    if (s & AI_META_SCHEME_IPTC)
        printf("  IPTC\n");
    if (s & AI_META_SCHEME_C2PA)
        printf("  C2PA (detect-only)\n");
    if (s & AI_META_SCHEME_UNKNOWN_AI)
        printf("  UNKNOWN_AI_HEURISTIC\n");
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }
    const char *cmd = argv[1];
    if (strcmp(cmd, "scan") == 0) {
        ai_meta_scan_result r;
        ai_meta_err err = ai_meta_scan_file(argv[2], &r);
        if (err != AI_META_OK) {
            fprintf(stderr, "error: %s\n", ai_meta_strerror(err));
            return 1;
        }
        printf("format: %s\nlikely_ai: %d\nschemes:\n", fmt_name(r.format), r.likely_ai);
        print_schemes(r.schemes);
        return 0;
    }
    if (strcmp(cmd, "extract") == 0) {
        ai_meta_info *info = NULL;
        ai_meta_err err = ai_meta_extract_file(argv[2], &info);
        if (err != AI_META_OK) {
            fprintf(stderr, "error: %s\n", ai_meta_strerror(err));
            return 1;
        }
        printf("%s\n", info->summary ? info->summary : "");
        for (size_t i = 0; i < info->field_count; i++) {
            printf("[%zu] %s = %.*s\n", i, info->fields[i].key, (int)info->fields[i].value_len,
                   info->fields[i].value ? info->fields[i].value : "");
        }
        ai_meta_info_free(info);
        return 0;
    }
    if (strcmp(cmd, "strip") == 0) {
        if (argc < 4) {
            usage(argv[0]);
            return 2;
        }
        unsigned flags = AI_META_FLAG_STRIP_ALL_AI | AI_META_FLAG_KEEP_COLOR_PROFILE;
        if (argc >= 5 && strcmp(argv[4], "--keep-non-ai") == 0)
            flags |= AI_META_FLAG_KEEP_NON_AI_TEXT;
        ai_meta_err err = ai_meta_strip_file(argv[2], argv[3], flags);
        if (err != AI_META_OK) {
            fprintf(stderr, "error: %s\n", ai_meta_strerror(err));
            return 1;
        }
        printf("wrote %s\n", argv[3]);
        return 0;
    }
    if (strcmp(cmd, "write") == 0) {
        if (argc < 6) {
            usage(argv[0]);
            return 2;
        }
        ai_meta_err err = ai_meta_write_file(argv[2], argv[3], argv[4], argv[5]);
        if (err != AI_META_OK) {
            fprintf(stderr, "error: %s\n", ai_meta_strerror(err));
            return 1;
        }
        printf("wrote %s\n", argv[3]);
        return 0;
    }
    usage(argv[0]);
    return 2;
}
