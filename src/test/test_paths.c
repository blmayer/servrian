char debug = 1;

#include "../aux.h"
#include <stdio.h>
#include <assert.h>


int main(void) {
	char *invalid_paths[] = {
		/* Original cases */
		".bashrc", "~/.ssh", "~", "../js",  "", "path/../pdf",
		/* New hardened cases */
		"/%2e%2e/etc/passwd",
		"/%252e%252e/etc/passwd",
		"/..%2fetc/passwd",
		"/..%2Fetc/passwd",
		"/foo%00bar",
		"/foo\\bar",
		"/etc/passwd%00.jpg",
		"/normal/../../secret",
		"/%2e%2e%2f%2e%2e%2fetc",
		"/foo%bar",           /* raw % */
		"/foo\nbar",
		"/foo\rbar",
		"/foo bar",
		"/../",
		"/%2e%2e/",
		".hidden",
		"../",
		"~user",
		"/very/long/path/that/exceeds/the/limit/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
		NULL
	};

	puts("testing paths");
	for (int i = 0; invalid_paths[i]; i++) {
		printf("%*s: ", 16, invalid_paths[i]);
		assert(invalid_path(invalid_paths[i]) != 0);
		puts("PASS");
	}

	/* Valid paths that should be allowed */
	char *valid_paths[] = {
		"/index.html",
		"/css/style.css",
		"/images/photo.jpg",
		"/js/app.js",
		"/subdir/file.txt",
		"/file-with-dash.txt",
		"/file_with_underscore.txt",
		"/path/to/nested/deep/resource.json",
		"/",
		NULL
	};

	puts("testing valid paths (should pass)");
	for (int i = 0; valid_paths[i]; i++) {
		printf("%*s: ", 16, valid_paths[i]);
		assert(invalid_path(valid_paths[i]) == 0);
		puts("PASS");
	}

	char *invalid_hosts[] = {
		/* Original weak cases */
		".bashrc", "~/.ssh", "~", "../js",  "", "path/../pdf", "/etc",
		/* New strong protection cases */
		"evil.com:22@attacker",          /* userinfo-like */
		"good.com@evil.com",             /* @ trick */
		"../etc",                        /* traversal */
		"%2e%2e/evil",                   /* encoded traversal */
		"evil%2ecom",                    /* percent in host */
		"evil.com:99999",                /* bad port */
		"evil.com:abc",                  /* non-numeric port */
		"evil.com:80:80",                /* multiple ports */
		"evil.com/path",                 /* path separator */
		"evil.com\\path",                /* backslash */
		"evil.com:80/..",                /* combined */
		"[::1]:badport",                 /* bad IPv6 port */
		"evil.com\x00",                  /* null byte (strlen stops but we still check) */
		"evil.com\n",                    /* newline / header injection */
		"evil.com\r",                    /* CR */
		"evil.com ",                     /* trailing space */
		" evil.com",                     /* leading space */
		"evil..com",                     /* double dot (even without /) */
		".evil.com",                     /* leading dot */
		"evil.com.",                     /* trailing dot */
		"-evil.com",                     /* leading dash */
		"evil.com-",                     /* trailing dash */
		"evil.com:0",                    /* port 0 */
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.com", /* >255 chars */
		NULL
	};

	puts("testing hosts");
	for (int i = 0; invalid_hosts[i]; i++) {
		printf("%*s: ", 16, invalid_hosts[i]);
		assert(invalid_host(invalid_hosts[i]) != 0);
		puts("PASS");
	}

	/* Positive cases that should now be allowed */
	char *valid_hosts[] = {
		"localhost",
		"localhost:8080",
		"example.com",
		"api.example.com",
		"192.168.1.10",
		"192.168.1.10:8443",
		"[::1]",
		"[::1]:8080",
		"[2001:db8::1]",
		"xn--bcher-kva.example",   /* IDN punycode */
		NULL
	};

	puts("testing valid hosts (should pass)");
	for (int i = 0; valid_hosts[i]; i++) {
		printf("%*s: ", 16, valid_hosts[i]);
		assert(invalid_host(valid_hosts[i]) == 0);
		puts("PASS");
	}
}
