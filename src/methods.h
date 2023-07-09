/*
 *  This is the function that sends the file requested in a response, it takes
 *  two arguments: the client's connection and the file requested in the GET
 *  message.
 */

#ifndef METHODS_H
#define METHODS_H

#include "webng.h"

/* Respond to a get request*/
int serve(int conn, struct request r);

/* Receives connection's parameters and serve a header */
int serve_status(int conn, struct request req, int status);

#endif
