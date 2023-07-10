#include "methods.h"
#include "aux.h"
#include "defs.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <unistd.h>

int serve(int conn, struct request r) {
	int status = 200;
	int clen = 0;
	int closeconn = 0;

	/* If / was passed, redirect to index page */
	char path[MAX_PATH_SIZE] = PAGES_DIR;
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
