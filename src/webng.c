#include "webng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void read_chunks(int conn, char *body) {
    /* Here we read and update the body */
    int chunk_size = 8; /* The size to be read */
    int body_size = 1;  /* This space is for the end 0 */
    char *chunk = malloc(chunk_size);
    int pos;

get_chunk:

    /* ---- Determine chunk size --------------------------------------- */

    pos = 0;
    while (read(conn, chunk + pos, 1) == 1) {

        /* The only thing that can break our loop is a line break */
        if (strcmp(chunk + pos, "\n") == 0) {
            break;
        }

        /* Increase pos by 1 to follow the buffer size */
        pos++;

        if (pos == chunk_size) {
            chunk_size += 8;
            chunk = realloc(chunk, chunk_size);
        }
    }
    sscanf(chunk, "%x", &pos); /* Hex of the chunk size */
    bzero(chunk, chunk_size);

    /* ---- Read chunk ------------------------------------------------- */

    if (pos > 0) {
        /* Allocate the size needed and read the chunk to body */
        body_size += pos;
        body = realloc(body, body_size);

        /* Now read the whole chunk, be sure */
        while (pos > 0) {
            pos -= read(conn, body + body_size - pos - 1, pos);
        }

        read(conn, chunk, 2); /* This will discard a \r\n */
        goto get_chunk;
    }

    free(chunk);
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

int parse_request(char *message, struct request *req) {
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

int req_header_len(struct request req) {
    /* URL request line, includes version, spaces and \r\n */
    int header_size = strlen(req.method) + strlen(req.url) + 12;

    header_size += 8 + strlen(req.host);  /* Host: \r\n */
    header_size += 14 + strlen(req.user); /* User-Agent: \r\n */
    header_size += 14 + strlen(req.conn); /* Connection: \r\n */
    header_size += 10 + strlen(req.cenc); /* Accept: \r\n */

    /* File length line, if we want */
    if (req.clen > 0) {
        /* Count the number of digits */
        int number = req.clen;
        int digits = 0;
        while (number != 0) {
            number /= 10;
            digits++;
        }
        header_size += digits + 18;            /* Number of digits */
        header_size += 14 + strlen(req.ctype); /* Content Encoding */
    }
    return header_size + 1; /* Terminating zero */
}

int create_req_header(struct request req, char *dest) {
    /* Copy all parameters to it */
    sprintf(dest,
            "%s %s HTTP/%.1f\r\n"
            "Host: %s\r\n"
            "User-Agent: %s\r\n"
            "Connection: %s\r\n"
            "Accept: %s\r\n",
            req.method, req.url, req.version, req.host, req.user, req.conn,
            req.cenc);

    if (req.clen > 0) {
        sprintf(dest + strlen(dest),
                "Content-Type: %s\r\n"
                "Content-Length: %d\r\n",
                req.ctype, req.clen);
    }

    return 0;
}

int create_res_header(struct response res, char *dest) {
    sprintf(dest,
            "HTTP/%.1f %hd %s\r\n"
            "Server: %s\r\nDate: %s\r\nConnection: %s\r\n"
            "Content-Type: %s\r\nContent-Length: %d\r\n\r\n",
            res.version, res.status, res.stext, res.server, res.date, res.conn,
            res.ctype, res.clen);

    return 0;
}
