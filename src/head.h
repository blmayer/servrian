#ifndef HEAD_INC
#define HEAD_INC

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "webng.h"
#include "aux.h"
#include "defs.h"

/* Receives connection's parameters and server a header */
int serve_head(short conn, struct request req, int status);

#endif

