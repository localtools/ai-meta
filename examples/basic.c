/* Build: cc -I../include -o basic basic.c ../build/libai_meta.a -lz */
#include "ai_meta.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <image>\n", argv[0]);
        return 2;
    }

    ai_meta_scan_result scan;
    if (ai_meta_scan_file(argv[1], &scan) != AI_META_OK) {
        fprintf(stderr, "scan failed\n");
        return 1;
    }
    printf("likely_ai=%d schemes=0x%x\n", scan.likely_ai, scan.schemes);

    ai_meta_info *info = NULL;
    if (ai_meta_extract_file(argv[1], &info) == AI_META_OK && info) {
        for (size_t i = 0; i < info->field_count; i++)
            printf("%s: %s\n", info->fields[i].key,
                   info->fields[i].value ? info->fields[i].value : "");
        ai_meta_info_free(info);
    }
    return 0;
}
