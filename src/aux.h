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

int hash(char *str);

/* File operations definitions */

int file_size(char *path);

char *load_file(char *path);

short write_log(char buff[512]);

/* Header processing tools */

char *date_line();

char *status_text(short status);

char *conn_text(int closeconn);

char *mime_type(char *ext);

#endif
