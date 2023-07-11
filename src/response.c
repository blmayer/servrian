#include "response.h"
#include "aux.h"
#include "defs.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/sendfile.h>
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
    // struct timeval tout = {10, 0};	/* Timeout structure: 3 mins */
    struct request req; /* Create our request structure */
    char header[MAX_HEADER_SIZE];

    /* Set the socket timeout */
    // setsockopt(cli_conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&tout, 18);

    /* ---- Read the request and respond ------------------------------ */

receive:
    /* Prepare variables to receive data */
    bzero(&req, sizeof(struct request));
    bzero(header, MAX_HEADER_SIZE);

    /* Read request */
    if (get_header(cli_conn, header) < 1) {
        return 0;
    }

    /* Populate our struct with request */
    if (parse_request(header, &req) < 0) {
        serve_status(cli_conn, req, 400);

        return 0;
    }

    /* Process the response with the correct method */
    switch (*req.method) {
    case 'H':
    case 'G':
        if (serve(cli_conn, req) < 0) {
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
	if (strstr(r.url, "..")) {
		return serve_status(conn, r, 400);
	}

	int status = 200;
	int clen = 0;
	int closeconn = 0;

	/* If / was passed, redirect to index page */
	char path[MAX_PATH_SIZE];
	strcpy(path, root);
	char *host = strtok(r.host, ":");
	strcat(path, host);
	if (strlen(r.url) == 1) {
		strcat(path, "/index.html");
	} else {
		strcat(path, r.url);
	}

	/* Verify the connection and request version */
	if (r.conn != NULL && (strcmp(r.conn, "Close") || r.version == 1)) {
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

	dprintf(
		conn,
		"HTTP/%.1f %d %s\r\n"
		"Server: " SERVER_NAME "\r\n"
		"Date: %s\r\n"
		"Connection: %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n\r\n",
		r.version, status, status_text(status), date_line(),
		conn_text(closeconn), mime_type(path), clen
	 );

	if (clen && strcmp(r.method, "HEAD")) {
		sendfile(conn, fileno(page_file), &(off_t){0}, clen);
	}
	fclose(page_file);

	return 0;
}

int serve_status(int conn, struct request req, int status) {
	return dprintf(
		conn,
		"HTTP/%.1f %d %s\r\n"
		"Server: " SERVER_NAME "\r\n"
		"Date: %s\r\n"
		"Connection: Close\r\n"
		"Content-Length: 0\r\n\r\n",
		req.version, status, status_text(status), date_line()
	 );
}
