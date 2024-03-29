#include "response.h"
#include "aux.h"
#include "defs.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>

extern char root[MAX_PATH_SIZE];

int get_header(int conn, char buffer[]) {
        int pos = 0;
        int n;

        /* This is a loop that will read the data coming from our connection */
        do {
                n = recv(conn, buffer + pos, MAX_HEADER_SIZE, 0);
                if (n < 1) {
                        return -1;
                }
                pos += n;

                /* The only thing that can break our loop is a blank line */
                if (strncmp(&buffer[pos - 4], "\r\n\r\n", 4) == 0) {
                        buffer[pos] = '\0';
                        break;
                }

        } while (n > 0);

        return pos;
}

int handle_request(int cli_conn) {
        /* Initialize variables for reading the request */
        struct request req; /* Create our request structure */
        char header[MAX_HEADER_SIZE];

        /* Set the socket timeout */
        // struct timeval tout = {10, 0};	/* Timeout structure: 3 mins */
        // setsockopt(cli_conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&tout, 18);

        /* ---- Read the request and respond ------------------------------ */

receive:
        /* prepare variables to receive data */
        bzero(&req, sizeof(struct request));
        bzero(header, MAX_HEADER_SIZE);

        /* read request */
        if (get_header(cli_conn, header) < 1) {
                return 0;
        }

        /* populate our struct with request */
        if (parse_request(header, &req) < 0) {
                serve_status(cli_conn, req, 400);
                return 0;
        }
        printf("%s: %s Host: %s Accept-Encoding: %s\n", req.method, req.url,
               req.host, req.cenc);

        /* some security checks */
        if (!req.host) {
                return serve_status(cli_conn, req, 400);
        }

        if (strstr(req.url, "..") || strstr(req.host, "..") ||
            *req.host == '/') {
                return serve_status(cli_conn, req, 400);
        }

        /* Process the response with the correct method */
        switch (*req.method) {
        case 'H':
        case 'G':
                if (serve(cli_conn, req) < 0) {
                        perror("unable to respond");
                }
                break;
        case 'P':
                /* post, put and patch will be handled here */
                if (ppp(cli_conn, req) < 0) {
                        perror("unable to respond");
                }
                break;
        default:
                if (serve_status(cli_conn, req, 501) < 0) {
                        perror("a problem occurred");
                }
        }

        /* Close connection depending on the case */
        if (req.conn != NULL && !strcmp(req.conn, "Keep-Alive")) {
                goto receive;
        }

        return 0;
}

int serve(int conn, struct request r) {
        int status = 200;
        int clen = 0;
        int closeconn = 0;

        /* If / was passed, redirect to index page */
        char path[MAX_PATH_SIZE];
        strcpy(path, root);
        strcat(path, r.host);
        if (strlen(r.url) == 1) {
                strcat(path, "/index.html");
        } else {
                strcat(path, r.url);
        }

        /* Check for compressed version */
        char compression[1] = "n";
        {
                char cpath[MAX_PATH_SIZE];
                if (r.cenc != NULL && strstr(r.cenc, "gzip")) {
                        strcpy(cpath, path);
                        strcat(cpath, ".gz");
                        if (access(cpath, F_OK) == 0) {
                                compression[0] = 'g';
                        }
                }
                if (r.cenc != NULL && strstr(r.cenc, "br")) {
                        strcpy(cpath, path);
                        strcat(cpath, ".br");
                        if (access(cpath, F_OK) == 0) {
                                compression[0] = 'b';
                        }
                }
                if (compression[0] != 'n') {
                        strcpy(path, cpath);
                }
        }
        printf("serving file %s\n", path);

        /* Verify the connection and request version */
        if (r.conn != NULL && (!strcmp(r.conn, "Close") || r.version == 1)) {
                closeconn = 1;
        }

        /* Read file and create response ---------------------------------- */

        FILE *page_file = fopen(path, "rb");
        if (page_file == NULL) {
                return serve_status(conn, r, 404);
        } else {
                fseek(page_file, 0, SEEK_END); /* Seek to the end */
                clen = ftell(page_file);       /* This position's the size */
                rewind(page_file);             /* Go back to the start */
        }

        dprintf(conn,
                "HTTP/%.1f %d %s\r\n"
                "Server: " SERVER_NAME "\r\n"
                "Date: %s\r\n"
                "Content-Length: %d\r\n",
                r.version, status, status_text(status), date_line(), clen);

        switch (compression[0]) {
        case 'g':
                /* remove .gz from path so the mimetype is correct */
                path[strlen(path) - 3] = 0;
                dprintf(conn, "Content-Encoding: gzip\r\n");
                break;
        case 'b':
                /* remove .br from path so the mimetype is correct */
                path[strlen(path) - 3] = 0;
                dprintf(conn, "Content-Encoding: br\r\n");
        }
        if (closeconn) {
                dprintf(conn, "Connection: Close\r\n");
        }
        dprintf(conn, "Content-Type: %s\r\n\r\n", mime_type(path));

        if (clen && strcmp(r.method, "HEAD")) {
                sendfile(conn, fileno(page_file), &(off_t){0}, clen);
        }
        fclose(page_file);

        return 0;
}

int serve_status(int conn, struct request req, int status) {
        return dprintf(conn,
                       "HTTP/%.1f %d %s\r\n"
                       "Server: " SERVER_NAME "\r\n"
                       "Date: %s\r\n"
                       "Connection: Close\r\n"
                       "Content-Length: 0\r\n\r\n",
                       req.version, status, status_text(status), date_line());
}

/*
 * This is how we handle post, patch and put requests:
 * We run an executable passing as arguments the compression mode and
 * the received body is sent to standard input.
 * The response is the executable's output, including the headers.
 * And the executable path is "root/Host/Path.sh"
 */
int ppp(int conn, struct request r) {
        int clen = 0;

        /* if / was passed, redirect to index page */
        char path[MAX_PATH_SIZE];
        strcpy(path, root);
        strcat(path, r.host);
        if (strlen(r.url) == 1) {
                strcat(path, "/index.html");
        } else {
                strcat(path, r.url);
        }
        strcat(path, ".sh ");
        strcat(path, r.method);

        /* check for compressed version */
        if (r.cenc != NULL && strstr(r.cenc, "gzip")) {
                strcat(path, " gzip");
        }
        if (r.cenc != NULL && strstr(r.cenc, "br")) {
                strcat(path, " br");
        }
        printf("running script %s\n", path);

        /* execute file and create response -------------------------------- */

	FILE *res = popen(path, "rw");
        if (!res) {
                return serve_status(conn, r, 404);
        }
	r.body = malloc(r.clen);
	read(conn, r.body, clen);
	write(conn, r.body, clen);
	free(r.body);

	char body[MAX_HEADER_SIZE] = {0};
	int n = 0;
	do {
		n = fread(body, 1, MAX_HEADER_SIZE, res);
		write(conn, &body, n);
		bzero(body, n);
	} while (n > 0);
	pclose(res);

        return 0;
}

