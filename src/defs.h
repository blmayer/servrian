#ifndef DEFS
#define DEFS

#define SERVER_NAME "Servrian v2.3.0"
#define MAX_HEADER_SIZE 8*1024
#define MAX_PATH_SIZE 256

#define STATUS_200 "OK"
#define STATUS_400 "Bad Request"
#define STATUS_401 "Unauthorized"
#define STATUS_404 "Not Found"
#define STATUS_500 "Internal Server Error"
#define STATUS_501 "Not Implemented"

#define CONN_CLOSE "Close"
#define CONN_KEEPA "Keep-Alive"

#define DEBUGF(args...) if (debug) {printf("[DEBUG] %s:%d: ", __FILE__, __LINE__); printf(args);}

struct url {
	char *proto;
	char *ip;
	char *port;
	char *domain;
	char *path;
	char *pars;
};

/* Our structure that contains the response's data */
struct response {
	short status;
	char *stext;
	char path[MAX_PATH_SIZE];
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
	char *auth;
	char *user;
	char *cenc;
	char *ctype;
	int clen;
	char *ttype;
	char *body;
};

#endif
