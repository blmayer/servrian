#include "aux.h"
#include "defs.h"
#include <crypt.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char debug;

/* Safe parsing helper - returns 0 on success, -1 on error.
 * Rejects negative numbers, overflow, and trailing garbage.
 */
static int parse_non_negative_int(const char *str, int *out, int max_val) {
        if (!str || !*str) return -1;

        char *endptr = NULL;
        long val = strtol(str, &endptr, 10);

        if (endptr == str) return -1;                    /* no digits */
        if (*endptr != '\0' && *endptr != '\r' && *endptr != '\n' && *endptr != ' ' && *endptr != '\t')
                return -1;                               /* trailing garbage */

        if (val < 0 || val > max_val) return -1;

        *out = (int)val;
        return 0;
}

/* Returns 1 if the string contains dangerous characters for HTTP parsing
 * (control chars, newlines, etc.). Used to harden strtok-based parsing.
 */
static int has_dangerous_chars(const char *s, size_t max_len) {
        if (!s) return 1;

        size_t len = strlen(s);
        if (len > max_len) return 1;

        for (size_t i = 0; i < len; i++) {
                unsigned char c = (unsigned char)s[i];
                if (c < 0x20 || c == 0x7f) return 1;   /* control chars */
        }
        return 0;
}

int invalid_path(char *p) {
        if (!p || !*p) return 1;

        size_t len = strlen(p);
        if (len == 0 || len > 255) return 1;

        /* Block traversal attempts (literal + common encoded bypasses) */
        if (strstr(p, "..") ||
            strstr(p, "%2e%2e") || strstr(p, "%2E%2E") ||
            strstr(p, "%252e") || strstr(p, "%252E") ||
            strstr(p, "%2f") || strstr(p, "%2F")) {
                return 1;
        }

        /* Block dangerous characters that can be used for traversal or injection */
        if (strchr(p, '\\') || strchr(p, '\0') || strchr(p, '%')) {
                return 1;
        }

        /* Block control characters and whitespace */
        for (size_t i = 0; i < len; i++) {
                unsigned char c = (unsigned char)p[i];
                if (c < 0x20 || c == 0x7f || isspace(c)) return 1;
        }

        /* Original intent: block paths starting with . or ~ */
        if (p[0] == '.' || p[0] == '~') return 1;

        return 0;
}

int invalid_host(char *h) {
        if (!h || !*h) return 1;

        size_t len = strlen(h);
        if (len == 0 || len > 255) return 1;

        /* Reject obvious traversal, path separators, encoding tricks, controls */
        if (strstr(h, "..") ||
            strchr(h, '/') || strchr(h, '\\') ||
            strchr(h, '%') || strchr(h, '\0')) {
                return 1;
        }

        /* Reject control characters and whitespace */
        for (size_t i = 0; i < len; i++) {
                unsigned char c = (unsigned char)h[i];
                if (c < 0x20 || c == 0x7f || isspace(c)) return 1;
        }

        /* Strict whitelist of safe characters for Host used as directory name */
        for (size_t i = 0; i < len; i++) {
                char c = h[i];
                if (!isalnum((unsigned char)c) &&
                    c != '.' && c != '-' && c != ':' &&
                    c != '[' && c != ']') {
                        return 1;
                }
        }

        /* Reject leading problematic characters */
        if (h[0] == '.' || h[0] == '-' || h[0] == ':' || h[0] == '[') {
                /* Allow leading '[' only for IPv6 literals */
                if (h[0] == '[' && len > 2 && h[len-1] == ']') {
                        /* looks like bare IPv6 literal [....] — acceptable */
                } else if (h[0] == '[') {
                        /* starts with [ but not a clean IPv6 literal */
                        return 1;
                } else {
                        return 1;
                }
        }

        /* Reject trailing dots or dashes (problematic for FS and DNS) */
        if (h[len-1] == '.' || h[len-1] == '-') return 1;

        /* Basic port sanity: if last colon exists and is not part of IPv6,
           the suffix after it should be all digits */
        char *last_colon = strrchr(h, ':');
        if (last_colon && last_colon != h) {
                /* If it contains '[', assume IPv6 form and be lenient with colons inside */
                if (!strchr(h, '[')) {
                        /* Plain host:port — everything after last : must be digits */
                        char *p = last_colon + 1;
                        if (*p == '\0') return 1;
                        for (; *p; p++) {
                                if (!isdigit((unsigned char)*p)) return 1;
                        }
                        /* Optional: could also atoi and check 1-65535, but length + digits is good enough */
                }
        }

        return 0;  /* appears safe to use as directory component */
}

int parse_URL(char *url, struct url *addr) {
        /* At first proto looks at the start of the URL */
        addr->proto = url;

        /* Try to match cases http://domain... and //domain... in URL */
        addr->domain = strstr(url, "://");
        if (addr->domain != NULL) {
                strcpy(addr->domain, "\0");
                addr->domain = addr->domain + 3;
        } else {
                /* URL starts with domain: www.domain... */
                addr->domain = url;
                addr->proto = NULL;
        }

        /* Try to find the port */
        addr->port = strstr(addr->domain, ":");
        if (addr->port == NULL) {
                addr->port = strstr(addr->domain, "/");
        } else {
                /* Writing \0 to delimit URL parts */
                strcpy(addr->port, "\0");
                addr->port++;
        }

        /* Search for a / */
        if (addr->port != NULL) {
                addr->path = strstr(addr->port, "/");
                if (addr->path != NULL) {
                        strcpy(addr->path, "\0");
                        addr->path++;

                        /* The last is ? */
                        addr->pars = strstr(addr->path, "?");
                } else {
                        addr->pars = strstr(addr->port, "?");
                }
        }

        /* Delimit path */
        if (addr->pars != NULL) {
                strcpy(addr->pars, "\0");
                addr->pars++;
        }

        return 0;
}

int parse_request(char message[MAX_HEADER_SIZE], struct request *req) {
        DEBUGF("parsing request\n");

        /* === Request Line Parsing (keep strtok for speed) === */
        req->method = strtok(message, " ");
        if (req->method == NULL || has_dangerous_chars(req->method, 16)) {
                puts("\tCould not parse the method or it contains dangerous chars.");
                return -1;
        }

        req->url = strtok(NULL, " ");
        if (req->url == NULL || has_dangerous_chars(req->url, 512)) {
                puts("\tCould not parse the URL or it is too long/dangerous.");
                return -1;
        }

        strtok(NULL, "/");   /* skip "HTTP/" */

        char *ver = strtok(NULL, "\r\n");
        if (ver == NULL || has_dangerous_chars(ver, 8)) {
                puts("\tCould not parse header's version.");
                return -1;
        }
        req->version = atof(ver);

        /* === Header Parsing === */
        char *temp = strtok(NULL, "\r\n");
        while (temp != NULL) {
                DEBUGF("parsing token %s\n", temp);

                /* Reject any header line that itself contains control chars or newlines
                   (defends against header injection / response splitting even with strtok) */
                if (has_dangerous_chars(temp, 4096)) {
                        puts("\tDangerous characters in header line.");
                        return -1;
                }

                if (strncmp(temp, "Host: ", 6) == 0) {
                        char *val = temp + 6;
                        while (*val == ' ' || *val == '\t') val++;
                        char *end = val + strlen(val);
                        while (end > val && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
                                *--end = '\0';
                        }
                        if (has_dangerous_chars(val, 256)) {
                                puts("\tDangerous Host header value.");
                                return -1;
                        }
                        req->host = val;
                        temp = strtok(NULL, "\r\n");
                        continue;
                }

                if (strncmp(temp, "User-Agent: ", 12) == 0) {
                        req->user = temp + 12;
                        temp = strtok(NULL, "\r\n");
                        continue;
                }
                if (strncmp(temp, "Authorization: ", 15) == 0) {
                        req->auth = temp + 15;
                        temp = strtok(NULL, "\r\n");
                        continue;
                }
                if (strncmp(temp, "Content-Length: ", 16) == 0) {
                        if (parse_non_negative_int(temp + 16, &req->clen, MAX_BODY_SIZE) != 0) {
                                req->clen = -1;
                        }
                        temp = strtok(NULL, "\r\n");
                        continue;
                }
                if (strncmp(temp, "Accept: ", 8) == 0) {
                        req->ctype = temp + 8;
                        temp = strtok(NULL, "\r\n");
                        continue;
                }
                if (strncmp(temp, "Accept-Encoding: ", 17) == 0) {
                        req->cenc = temp + 17;
                        temp = strtok(NULL, "\r\n");
                        continue;
                }
                if (strncmp(temp, "Connection: ", 12) == 0) {
                        req->conn = temp + 12;
                        temp = strtok(NULL, "\r\n");
                        continue;
                }

                temp = strtok(NULL, "\r\n");
        }

        DEBUGF("parsed request\n");
        return 0;
}

int hash(char *str) {
        unsigned int hash = 5381;
        int c;
        while ((c = *str++)) {
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        }
        return (int)hash;
}

/* Header processing tools */

char *date_line() {
        /* Get the current time in the correct format */
        struct tm *cur_time; /* Obtain current time */
        time_t now = time(NULL);
        cur_time = gmtime(&now);
        static char res_time[30]; /* Convert to local time format */

        /* Date: Fri, 19 Feb 1992 08:53:17 GMT */
        strftime(res_time, 30, "%a, %d %b %Y %X %Z", cur_time);

        return res_time;
}

char *status_text(short status) {
        switch (status) {
        case 200:
                return STATUS_200;
        case 400:
                return STATUS_400;
        case 401:
                return STATUS_401;
        case 404:
                return STATUS_404;
        case 500:
                return STATUS_500;
        case 501:
                return STATUS_501;
        default:
                return "Unknown";
        }
}

char *conn_text(int closeconn) { return closeconn ? CONN_CLOSE : CONN_KEEPA; }

char *mime_type(char *path) {
        switch (hash(strrchr(path, '.'))) {
        case 192572072: /* .html */
                return "text/html";
        case 2088238428: /* .css */
                return "text/css";
        case 192642925: /* .json */
                return "text/json";
        case 2088245940: /* .jpg */
        case 192639321: /* .jpeg */
                return "image/jpeg";
        case 193430704: /* .js */
                return "application/javascript";
        case 2088252408: /* .png */
                return "image/png";
        case 2088252077: /* .pdf */
                return "application/pdf";
        case 2088244430: /* .ico */
                return "image/x-icon";
        case 193105445: /* .woff */
                return "application/x-font-woff";
        case 2088255939: /* .svg */
                return "image/svg+xml";
        case 2088261092: /* .xml */
                return "text/xml";
        case 2088257107: /* .txt */
                return "text/plain";
        case 2088249156: /* .mp4 */
                return "video/mp4";
        case 192838546: /* .pack */
                return "application/x-git-packed-objects";
        case 2088244472: /* .idx */
                return "application/octet-stream";
        default:
                return "text/html";
        }
}

static const char table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int isbase64(char c) { return c && strchr(table, c) != NULL; }

char value(char c) {
        const char *p = strchr(table, c);
        if (p) {
                return p - table;
        } else {
                return 0;
        }
}

int base64_decode(char *src, char *dest, size_t dest_size) {
        if (!src || !dest || dest_size < 1) {
                return -1;
        }
        if (*src == 0) {
                dest[0] = 0;
                return 0;
        }

        size_t srclen = strlen(src);
        char *p = dest;
        size_t written = 0;

        do {
                if (written + 3 >= dest_size) {
                        /* Would overflow destination buffer */
                        *p = 0;
                        return -1;
                }

                char a = value(src[0]);
                char b = value(src[1]);
                char c = value(src[2]);
                char d = value(src[3]);

                *p++ = (a << 2) | (b >> 4);
                *p++ = (b << 4) | (c >> 2);
                *p++ = (c << 6) | d;
                written += 3;

                if (!isbase64(src[1])) {
                        p -= 2;
                        written -= 2;
                        break;
                } else if (!isbase64(src[2])) {
                        p -= 2;
                        written -= 2;
                        break;
                } else if (!isbase64(src[3])) {
                        p--;
                        written -= 1;
                        break;
                }

                src += 4;
                while (*src && (*src == 13 || *src == 10))
                        src++;

        } while (srclen -= 4);

        *p = 0;
        return (int)written;
}

int pw_check(struct passwd *pw, char *pass) {
        char *cryptpass, *p;
        struct spwd *spw;

        p = pw->pw_passwd;
        if (p[0] == '!' || p[0] == '*') {
                return errno;
        }

        if (pw->pw_passwd[0] == '\0') {
                if (pass[0] == '\0')
                        return 1;
                return 0;
        }

        if (pw->pw_passwd[0] == 'x' && pw->pw_passwd[1] == '\0') {
                spw = getspnam(pw->pw_name);
                if (!spw) {
                        return errno;
                }
                p = spw->sp_pwdp;
                if (p[0] == '!' || p[0] == '*') {
                        return errno;
                }
        }

        cryptpass = crypt(pass, p);
        if (!cryptpass) {
                return errno;
        }
        if (strcmp(cryptpass, p) != 0) {
                return 0;
        }
        return 1;
}
