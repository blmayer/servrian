#include "defs.h"
#include "webng.h"
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

int parse_response(char *message, struct response *res) {
    /* ---- Get first line parameters ---------------------------------- */

    /* Due to atof we need to test for a NULL pointer */
    char *ver = strtok(message, " ");
    if (ver != NULL) {
        /* The HTTP version */
        res->version = atof(ver + 5);
    } else {
        puts("\tCould not parse header's version.");
        return -1;
    }

    /* Advance to the status */
    char *stat = strtok(NULL, " ");
    if (stat != NULL) {
        /* Lastly the HTTP status */
        res->status = atoi(stat);
    } else {
        puts("\tCould not parse header's status.");
        return -1;
    }

    /* Put pointer in next line */
    char *temp = strtok(NULL, "\r\n");

    while (temp != NULL) {
        /* Keep advancing in string getting some parameters */
        if (strncmp(temp, "Server: ", 8) == 0) {
            res->server = temp + 8;
            temp = strtok(NULL, "\r\n");
            continue;
        }
        if (strncmp(temp, "Date: ", 6) == 0) {
            res->date = temp + 6;
            temp = strtok(NULL, "\r\n");
            continue;
        }
        if (strncmp(temp, "Content-Length: ", 16) == 0) {
            res->clen = atoi(temp + 16);
            temp = strtok(NULL, "\r\n");
            continue;
        }
        if (strncmp(temp, "Content-Type: ", 14) == 0) {
            res->ctype = temp + 14;
            temp = strtok(NULL, "\r\n");
            continue;
        }
        if (strncmp(temp, "Content-Encoding: ", 18) == 0) {
            res->cenc = temp + 18;
            temp = strtok(NULL, "\r\n");
            continue;
        }
        if (strncmp(temp, "Connection: ", 12) == 0) {
            res->conn = temp + 12;
            temp = strtok(NULL, "\r\n");
            continue;
        }
        if (strncmp(temp, "Transfer-Encoding: ", 19) == 0) {
            res->ttype = temp + 19;
            temp = strtok(NULL, "\r\n");
            continue;
        }
        temp = strtok(NULL, "\r\n");
    }

    return 0;
}

