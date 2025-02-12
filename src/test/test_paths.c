char debug = 1;

#include "../aux.h"
#include <stdio.h>
#include <assert.h>


int main(void) {
	char *invalid_paths[] = {
		".bashrc", "~/.ssh", "~", "/etc", "../js",  "", "path/../pdf",
		NULL
	};

	for (int i = 0; invalid_paths[i]; i++) {
		printf("%*s: ", 16, invalid_paths[i]);
		assert(invalid_path(invalid_paths[i]) != 0);
		puts("PASS");
	}
}
