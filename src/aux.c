#include "aux.h"
#include "defs.h"
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char debug;

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

        /* Get first line parameters */
        req->method = strtok(message, " "); /* First token is the method */
        if (req->method == NULL) {
                puts("\tCould not parse the method.");
                return -1;
        }
        DEBUGF("parsed method %s\n", req->method);

        req->url = strtok(NULL, " "); /* Then the url or path */
        strtok(NULL, "/");            /* Advance to the version no */

        /* Due to atof we need to test for a NULL pointer */
        char *ver = strtok(NULL, "\r\n");
        if (ver != NULL) {
                req->version = atof(ver); /* Lastly the HTTP version */
        } else {
                puts("\tCould not parse header's version.");
                return -1;
        }

        /* Put pointer in next line */
        char *temp = strtok(NULL, "\r\n");
        while (temp != NULL) {
                DEBUGF("parsing token %s\n", temp);

                /* Keep advancing in string getting some parameters */
                if (strncmp(temp, "Host: ", 6) == 0) {
                        req->host = temp + 6;
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
                        req->clen = atoi(temp + 16);
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
        int res = 0;
        int pos = 0;

        while (str[pos]) {
                res += str[pos] + pos;
                pos++;
        }
        return res;
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
        case 270:
                return "application/javascript";
        case 367:
                return "image/x-icon";
        case 377:
                return "image/png";
        case 381:
                return "text/css";
        case 388:
                return "image/svg+xml";
        case 389:
                return "text/xml";
        case 395:
                return "text/xsl";
        case 404:
                return "text/plain";
        case 490:
                return "application/x-font-woff";
        case 493:
                return "text/html";
        case 498:
                return "text/json";
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

int base64_decode(char *src, char dest[MAX_PATH_SIZE]) {
        if (*src == 0) {
                return 0;
        }
        int srclen = strlen(src);

        char *p = dest;
        do {

                char a = value(src[0]);
                char b = value(src[1]);
                char c = value(src[2]);
                char d = value(src[3]);
                *p++ = (a << 2) | (b >> 4);
                *p++ = (b << 4) | (c >> 2);
                *p++ = (c << 6) | d;
                if (!isbase64(src[1])) {
                        p -= 2;
                        break;
                } else if (!isbase64(src[2])) {
                        p -= 2;
                        break;
                } else if (!isbase64(src[3])) {
                        p--;
                        break;
                }
                src += 4;
                while (*src && (*src == 13 || *src == 10))
                        src++;
        } while (srclen -= 4);
        *p = 0;
        return p - dest;
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
