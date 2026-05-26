/*
 *  Here we have auxiliary functions intended to be used by other functions.
 */

#ifndef AUXFNS_H
#define AUXFNS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include "defs.h"

int invalid_path(char *path);

int invalid_host(char *path);

int hash(char *str);

int parse_request(char message[MAX_HEADER_SIZE], struct request *req);

/* Header processing tools */

char *date_line();

char *status_text(short status);

char *conn_text(int closeconn);

char *mime_type(char *ext);

/* Safe base64 decode. Returns number of bytes written, or -1 on error/overflow. */
int base64_decode(char *src, char *dest, size_t dest_size);

int pw_check(struct passwd *pw, char *pass);

#endif
