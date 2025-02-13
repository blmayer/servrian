char debug = 1;

#include "../aux.h"
#include <stdio.h>
#include <assert.h>


int main(void) {
	char *invalid_paths[] = {
		".bashrc", "~/.ssh", "~", "../js",  "", "path/../pdf",
		NULL
	};

	puts("testing paths");
	for (int i = 0; invalid_paths[i]; i++) {
		printf("%*s: ", 16, invalid_paths[i]);
		assert(invalid_path(invalid_paths[i]) != 0);
		puts("PASS");
	}

	char *invalid_hosts[] = {
		".bashrc", "~/.ssh", "~", "../js",  "", "path/../pdf", "/etc",
		NULL
	};

	puts("testing hosts");
	for (int i = 0; invalid_hosts[i]; i++) {
		printf("%*s: ", 16, invalid_hosts[i]);
		assert(invalid_host(invalid_hosts[i]) != 0);
		puts("PASS");
	}
}
