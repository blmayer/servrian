#include "aux.h"
#include "defs.h"
#include <string.h>

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

char *conn_text(int closeconn) {
	return closeconn ? CONN_CLOSE : CONN_KEEPA;
}

char *mime_type(char *path) {
    /* Match the extension against some cases */
    char *mime;
    switch (hash(strrchr(path, '.'))) {
    case 493:
        mime = "text/html";
        break;
    case 381:
        mime = "text/css";
        break;
    case 270:
        mime = "application/javascript";
        break;
    case 377:
        mime = "image/png";
        break;
    case 388:
        mime = "image/svg+xml";
        break;
    case 367:
        mime = "image/x-icon";
        break;
    case 490:
        mime = "application/x-font-woff";
        break;
    default:
        mime = "application/octet-stream";
        break;
    }

    return mime;
}
