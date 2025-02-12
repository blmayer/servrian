char debug = 1;
#include "../aux.h"
#include <stdio.h>


int main(void) {
	char *exts[] = {
		".html", ".css",  ".json",   ".jpg",  ".js",  ".png", ".pdf",
                ".ico",  ".woff", ".svg",    ".jpeg", ".xml", ".txt",
		"GET", "HEAD",  "POST",  "OPTIONS", NULL
	};

	puts("+-----------+-----------+");
	puts("|    ext\thash    |");
	puts("+-----------+-----------+");
	for (int i = 0; exts[i]; i++) {
		printf("| %*s\t%d\t|\n", 8, exts[i], hash(exts[i]));
	}
	puts("+-----------------------+");
}
