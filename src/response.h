#ifndef RECEIVE_H
#define RECEIVE_H

#include <sys/socket.h>
#include <unistd.h>
#include "webng.h"
#include "head.h"
#include "get.h"

/* This function reads the header and passes it to the right function */
int handle_request(int cli_conn);

#endif

