#include "../aux.h"
#include <stdio.h>

int main(void) {
    char *exts[] = {".html", ".css",  ".json",   ".jpg",  ".js",  ".png",
                    ".ico",  ".woff", ".svg",    ".jpeg", ".xml", "GET",
                    "HEAD",  "POST",  "OPTIONS", NULL};

    puts("ext\thash");
    puts("------------");
    for (int i = 0; exts[i]; i++) {
        printf("%s\t%d\n", exts[i], hash(exts[i]));
    }
}