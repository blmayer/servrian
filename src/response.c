#include "response.h"
#include "defs.h"
#include <stdio.h>
#include <strings.h>
#include <sys/socket.h>

int get_header(int conn, char buffer[]) {
    int pos = 0;
    int n;

    /* This is a loop that will read the data coming from our connection */
    do {
        n = recv(conn, buffer + pos, MAX_HEADER_SIZE, 0);
        if (n == -1) {
            return -1;
        }
        pos += n;

        /* The only thing that can break our loop is a blank line */
        if (strncmp(buffer + pos - 4, "\r\n\r\n", 4) == 0) {
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
        serve_head(cli_conn, req, 400);

        return 0;
    }

    /* Process the response with the correct method */
    switch (strcmp(req.method, "PEZ")) {
    case -8:
        if (serve_head(cli_conn, req, 200) < 0) {
            perror("unable to respond");
        }

        break;

    case -9:
        if (serve_get(cli_conn, req) < 0) {
            perror("a problem occurred");
        }

        break;

    default:
        if (serve_head(cli_conn, req, 501) < 0) {
            perror("a problem occurred");
        }
    }

    /* Close connection depending on the case */
    if (req.conn == NULL && req.version > 1) {
        goto receive;
    } else if (req.conn != NULL && strcmp(req.conn, "Close")) {
        goto receive;
    }

    return 0;
}
