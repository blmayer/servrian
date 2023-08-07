#include "aux.h"
#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    /* Get first line parameters */
    req->method = strtok(message, " "); /* First token is the method */
    if (req->method == NULL) {
        puts("\tCould not parse the method.");
        return -1;
    }

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
        if (strncmp(temp, "Content-Length: ", 16) == 0) {
            req->clen = atoi(temp + 16);
            temp = strtok(NULL, "\r\n");
            continue;
        }
        if (strncmp(temp, "Content-Type: ", 14) == 0) {
            req->ctype = temp + 14;
            temp = strtok(NULL, "\r\n");
            continue;
        }
        if (strncmp(temp, "Content-Encoding: ", 18) == 0) {
            req->cenc = temp + 18;
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
	switch (hash(strrchr(path, '.'))) {
		case 493:
			return "text/html";
		case 389:
			return "text/xml";
		case 395:
			return "text/xsl";
		case 381:
			return "text/css";
		case 270:
			return "application/javascript";
		case 377:
			return "image/png";
		case 388:
			return "image/svg+xml";
		case 367:
			return "image/x-icon";
		case 490:
			return "application/x-font-woff";
		default:
			return "application/octet-stream";
	}
}
