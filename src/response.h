#ifndef RECEIVE_H
#define RECEIVE_H

#include "defs.h"

/* This function reads the header and passes it to the right function */
int handle_request(int cli_conn);

int serve(int conn, struct request r);

int serve_status(int conn, struct request req, int status, char *extra);

int ppp(int conn, struct request r);

#endif
