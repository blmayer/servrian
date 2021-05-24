/*
 *  This is the file that defines our structures used in our functions, here
 *  we have the cryptographic key, encrypting requests; the request struct
 *  to hold the information pertaining to a request; and the response struct
 *  holding the response information.
 */

#ifndef WEBNG_H
#define WEBNG_H

/*
 * Objects definitions
 */

/* Our structure that contains the response's data */
struct response {
    short status;
    char *stext;
    char *path;
    float version;
    char *server;
    char *date;
    char *conn;
    char *cenc;
    char *ctype;
    int clen;
    char *ttype;
    char *body;
};

/* Our structure that contains the request's data */
struct request {
    char *method;
    char *url;
    float version;
    char *host;
    char *conn;
    char *user;
    char *cenc;
    char *ctype;
    int clen;
    char *ttype;
    char *body;
};

struct url {
    char *proto;
    char *ip;
    char *port;
    char *domain;
    char *path;
    char *pars;
};

/*
 * Function prototypes
 */

/* Reads data in chunked format from a socket */
void read_chunks(int conn, char *body);

/* Extracts a token from a string */
char *get_token(char *source, char *par);

/* Extracts the path from an URL */
int parse_URL(char *url, struct url *addr);

/* Parses a string and populates the request structure */
int parse_request(char *message, struct request *req);

/* Parses a string and populates the response structure */
int parse_response(char *message, struct response *res);

/* Calculates a request header size */
int req_header_len(struct request req);

/* Creates a HTTP header string */
int create_req_header(struct request req, char *dest);

/* Creates a response with header and optional body */
int create_res_header(struct response res, char *dest);

#endif
