#include "response.h"
#include "aux.h"
#include "defs.h"
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

extern char root[MAX_PATH_SIZE];
extern char debug;
extern uid_t hostuid;
extern gid_t hostgid;

/* Connection-level timeout (applies to both reading requests and writing responses) */
#define CONNECTION_TIMEOUT_SEC 30

/* Defense-in-depth: canonical root for path escape prevention */
static char canonical_root[MAX_PATH_SIZE] = {0};
static int   canonical_root_len = 0;

static void init_canonical_root(void) {
    if (canonical_root_len == 0) {
        if (realpath(root, canonical_root) != NULL) {
            canonical_root_len = strlen(canonical_root);
            /* Ensure it ends with / for easy prefix matching */
            if (canonical_root_len > 0 && canonical_root[canonical_root_len-1] != '/') {
                if (canonical_root_len + 1 < MAX_PATH_SIZE) {
                    strcat(canonical_root, "/");
                    canonical_root_len++;
                }
            }
        } else {
            /* Fallback: use the raw root */
            strncpy(canonical_root, root, MAX_PATH_SIZE - 1);
            canonical_root[MAX_PATH_SIZE - 1] = 0;
            canonical_root_len = strlen(canonical_root);
        }
    }
}

/* Returns 1 if the candidate path is safely under the web root.
 * Uses realpath() for defense-in-depth against traversal and symlink attacks.
 */
static int path_is_safe(const char *candidate, char *resolved, size_t resolved_size) {
    init_canonical_root();

    if (realpath(candidate, resolved) == NULL) {
        /* File/directory does not exist.
         * For non-existing paths we still want to prevent obvious escapes.
         * A conservative approach: if the raw candidate contains ".." after
         * our earlier checks, reject. Since we already ran invalid_path(),
         * we can be lenient here and let the caller return 404.
         */
        return 1;   // Let later stat()/fopen() produce 404
    }

    /* Check that the resolved path starts with our canonical root */
    if (strncmp(resolved, canonical_root, canonical_root_len) != 0) {
        return 0;   // Escape attempt or symlink outside root
    }

    return 1;
}

int get_header(int conn, char buffer[], size_t max_size) {
        DEBUGF("reading request header\n");

        if (max_size < 4) {
                return -1;
        }

        int pos = 4;
        if (recv(conn, buffer, 4, 0) != 4) {
                return -1;
        }

        while (pos < (int)max_size - 1) {
                if (strncmp(&buffer[pos - 4], "\r\n\r\n", 4) == 0) {
                        break;
                }
                if (recv(conn, buffer + pos, 1, 0) < 1) {
                        return -1;
                }
                pos++;
        }

        /* Check if we ran out of space without finding the terminator */
        if (pos >= (int)max_size - 1 &&
            strncmp(&buffer[pos - 4], "\r\n\r\n", 4) != 0) {
                return -2;  /* Header too large */
        }

        buffer[pos] = '\0';
        return pos;
}

int handle_request(int cli_conn) {

        /* Initialize variables for reading the request */
        struct request req; /* Create our request structure */
        char header[MAX_HEADER_SIZE];

        /* Set connection-level timeouts to prevent slowloris / hanging connections.
         * Note: This protects the client socket. Long-running PPP scripts can still
         * block the pipe read in the parent — a separate script timeout would be needed
         * for full protection.
         */
        {
                struct timeval tout = { CONNECTION_TIMEOUT_SEC, 0 };
                setsockopt(cli_conn, SOL_SOCKET, SO_RCVTIMEO, &tout, sizeof(tout));
                setsockopt(cli_conn, SOL_SOCKET, SO_SNDTIMEO, &tout, sizeof(tout));
        }

        /* ---- Read the request and respond ------------------------------ */

receive:
        /* prepare variables to receive data */
        bzero(&req, sizeof(struct request));
        bzero(header, MAX_HEADER_SIZE);

        /* read request */
        int header_len = get_header(cli_conn, header, MAX_HEADER_SIZE);
        if (header_len < 0) {
                if (header_len == -2) {
                        /* Header too large - send 400 if possible */
                        dprintf(cli_conn,
                                "HTTP/1.1 400 Bad Request\r\n"
                                "Connection: Close\r\n"
                                "Content-Length: 0\r\n\r\n");
                }
                return 0;
        }

        /* populate our struct with request */
        if (parse_request(header, &req) < 0) {
                serve_status(cli_conn, req, 400, "");
                return 0;
        }

        printf("method=%s url=%s Host=%s Accept-Encoding=%s Agent=%s\n", req.method,
               req.url, req.host, req.cenc, req.user);

        /* some security checks */
        if (invalid_host(req.host) || invalid_path(req.url)) {
                return serve_status(cli_conn, req, 400, "");
        }

        /* Hard limit on request body size (especially important for PPP) */
        if (req.clen < 0 || req.clen > MAX_BODY_SIZE) {
                return serve_status(cli_conn, req, 400, "");
        }

        /* Process the response with the correct method */
        switch (*req.method) {
        case 'H':
        case 'G':
                if (serve(cli_conn, req) < 0) {
                        perror("unable to respond");
                }
                break;
        case 'P':
                /* post, put and patch will be handled here */
                if (ppp(cli_conn, req) < 0) {
                        perror("unable to respond");
                }
                break;
        default:
                if (serve_status(cli_conn, req, 501, "") < 0) {
                        perror("a problem occurred");
                }
        }

        /* Close connection depending on the case */
        if (req.conn != NULL && !strcmp(req.conn, "Keep-Alive")) {
                goto receive;
        }

        return 0;
}

int serve(int conn, struct request r) {
        DEBUGF("handling GET request\n");
        int status = 200;
        int clen = 0;
        int closeconn = 0;

        /* If / was passed, redirect to index page */
        char path[MAX_PATH_SIZE];
        strcpy(path, root);
        strcat(path, r.host);
        if (strlen(r.url) == 1) {
                strcat(path, "/index.html");
        } else {
                strcat(path, r.url);
        }

        /* Check for compressed version */
        char compression[1] = "n";
        {
                char cpath[MAX_PATH_SIZE];
                if (r.cenc != NULL && strstr(r.cenc, "gzip")) {
                        strcpy(cpath, path);
                        strcat(cpath, ".gz");
                        if (access(cpath, F_OK) == 0) {
                                compression[0] = 'g';
                        }
                }
                if (r.cenc != NULL && strstr(r.cenc, "br")) {
                        strcpy(cpath, path);
                        strcat(cpath, ".br");
                        if (access(cpath, F_OK) == 0) {
                                compression[0] = 'b';
                        }
                }
                if (compression[0] != 'n') {
                        strcpy(path, cpath);
                }
        }

        /* === Defense-in-depth path escape check === */
        char resolved[MAX_PATH_SIZE];
        if (!path_is_safe(path, resolved, sizeof(resolved))) {
                return serve_status(conn, r, 403, "");
        }
        /* Use the resolved (canonical) path from now on */
        strncpy(path, resolved, MAX_PATH_SIZE - 1);
        path[MAX_PATH_SIZE - 1] = 0;

        printf("serving file %s\n", path);

        /* Verify the connection and request version */
        if (r.conn != NULL && (!strcmp(r.conn, "Close") || r.version == 1)) {
                closeconn = 1;
        }

        /* Read file and create response ---------------------------------- */

        struct stat stats;
        if (stat(path, &stats) < 0) {
                return serve_status(conn, r, 404, "");
        }

        /* check permissions */
        if (!(stats.st_mode & S_IROTH)) {
                puts("file needs authorization");
                if (!r.auth || strncmp(r.auth, "Basic", 5)) {
                        return serve_status(conn, r, 401,
                                            "WWW-Authenticate: Basic\r\n");
                }

                r.auth += 6;
                char userpass[MAX_PATH_SIZE];
                if (base64_decode(r.auth, userpass, sizeof(userpass)) < 0) {
                        return serve_status(conn, r, 401,
                                            "WWW-Authenticate: Basic\r\n");
                }
                char *user = strtok(userpass, ":");
                char *pass = strtok(NULL, ":");

                /* check passwd */
                struct passwd *pw = getpwnam(user);
                if (!pw) {
                        puts("getpwnam error");
                        return serve_status(conn, r, 401,
                                            "WWW-Authenticate: Basic\r\n");
                }

                int res = pw_check(pw, pass);
		if (res <= 0) {
                        printf("password didn't check: %d", res);
                        return serve_status(conn, r, 401,
                                            "WWW-Authenticate: Basic\r\n");
                }

                if (setgid(pw->pw_gid) < 0)
                        return serve_status(conn, r, 500, "");
                if (setuid(pw->pw_uid) < 0)
                        return serve_status(conn, r, 500, "");

                printf("%s authorized\n", user);
        }

        FILE *page_file = fopen(path, "rb");
        if (page_file == NULL) {
                return serve_status(conn, r, 404, "");
        } else {
                fseek(page_file, 0, SEEK_END); /* Seek to the end */
                clen = ftell(page_file);       /* This position's the size */
                rewind(page_file);             /* Go back to the start */
        }

        dprintf(conn,
                "HTTP/%.1f %d %s\r\n"
                "Server: " SERVER_NAME "\r\n"
                "Date: %s\r\n"
                "Content-Length: %d\r\n",
                r.version, status, status_text(status), date_line(), clen);

        switch (compression[0]) {
        case 'g':
                /* remove .gz from path so the mimetype is correct */
                path[strlen(path) - 3] = 0;
                dprintf(conn, "Content-Encoding: gzip\r\n");
                break;
        case 'b':
                /* remove .br from path so the mimetype is correct */
                path[strlen(path) - 3] = 0;
                dprintf(conn, "Content-Encoding: br\r\n");
        }
        if (closeconn) {
                dprintf(conn, "Connection: Close\r\n");
        }
        dprintf(conn, "Content-Type: %s\r\n\r\n", mime_type(path));

        if (clen && strcmp(r.method, "HEAD")) {
                sendfile(conn, fileno(page_file), &(off_t){0}, clen);
        }
        fclose(page_file);

        setuid(hostuid);
        setgid(hostgid);

        return 0;
}

int serve_status(int conn, struct request req, int status, char *extra) {
        return dprintf(conn,
                       "HTTP/%.1f %d %s\r\n"
                       "Server: " SERVER_NAME "\r\n"
                       "Date: %s\r\n"
                       "%s"
                       "Connection: Close\r\n"
                       "Content-Length: 0\r\n\r\n",
                       req.version, status, status_text(status), date_line(),
                       extra);
}

/*
 * This is how we handle post, patch and put requests:
 * We run an executable passing as arguments the compression mode and
 * the received body is sent to standard input.
 * The response is the executable's output, including the headers.
 * And the executable path is "root/Host/Path.sh"
 */
int ppp(int conn, struct request r) {
        DEBUGF("handling %s request\n", r.method);

        /* Build path to the script (same style as serve()) */
        char path[MAX_PATH_SIZE];
        strcpy(path, root);
        strcat(path, r.host);
        if (strlen(r.url) == 1) {
                strcat(path, "/index.html");
        } else {
                strcat(path, r.url);
        }
        strcat(path, ".sh");
        DEBUGF("running script %s\n", path);

        /* === Defense-in-depth path escape check (same as serve()) === */
        char resolved[MAX_PATH_SIZE];
        if (!path_is_safe(path, resolved, sizeof(resolved))) {
                return serve_status(conn, r, 403, "");
        }
        strncpy(path, resolved, MAX_PATH_SIZE - 1);
        path[MAX_PATH_SIZE - 1] = 0;

        /* === Same permission + auth checks as regular file serving === */
        struct stat stats;
        if (stat(path, &stats) < 0) {
                return serve_status(conn, r, 404, "");
        }

        uid_t run_as_uid = hostuid;
        gid_t run_as_gid = hostgid;

        if (!(stats.st_mode & S_IROTH)) {
                puts("PPP script needs authorization");
                if (!r.auth || strncmp(r.auth, "Basic", 5)) {
                        return serve_status(conn, r, 401,
                                            "WWW-Authenticate: Basic\r\n");
                }

                r.auth += 6;
                char userpass[MAX_PATH_SIZE];
                if (base64_decode(r.auth, userpass, sizeof(userpass)) < 0) {
                        return serve_status(conn, r, 401,
                                            "WWW-Authenticate: Basic\r\n");
                }

                char *user = strtok(userpass, ":");
                char *pass = strtok(NULL, ":");

                struct passwd *pw = getpwnam(user);
                if (!pw) {
                        puts("getpwnam error (PPP)");
                        return serve_status(conn, r, 401,
                                            "WWW-Authenticate: Basic\r\n");
                }

                if (pw_check(pw, pass) <= 0) {
                        printf("password didn't check for PPP: %s\n", user);
                        return serve_status(conn, r, 401,
                                            "WWW-Authenticate: Basic\r\n");
                }

                run_as_uid = pw->pw_uid;
                run_as_gid = pw->pw_gid;
                printf("%s authorized for PPP script\n", user);
        }

        /* === Execute the script (with optional privilege drop) === */
        int inpipefd[2];
        int outpipefd[2];

        if (pipe(inpipefd) < 0 || pipe(outpipefd) < 0) {
                return serve_status(conn, r, 500, "");
        }

        pid_t pid = fork();
        if (pid == 0) {
                // Child: script executor
                dup2(outpipefd[0], STDIN_FILENO);
                dup2(inpipefd[1], STDOUT_FILENO);

                close(outpipefd[1]);
                close(inpipefd[0]);

                /* Drop privileges if auth was required (same as serve()) */
                if (run_as_uid != hostuid) {
                        setgid(run_as_gid);
                        setuid(run_as_uid);
                }

                execl(path, path, r.method, r.cenc, (char *)NULL);
                exit(1);
        }

        // Parent side (connection handler)
        close(outpipefd[0]);
        close(inpipefd[1]);

        if (r.clen > MAX_BODY_SIZE || r.clen < 0) {
                close(outpipefd[1]);
                close(inpipefd[0]);
                return 0;
        }

        r.body = malloc(r.clen + 1);
        if (r.body) {
                read(conn, r.body, r.clen);
                r.body[r.clen] = 0;
                write(outpipefd[1], r.body, r.clen);
        }
        close(outpipefd[1]);
        free(r.body);

        char body[MAX_HEADER_SIZE];
        ssize_t n;
        while ((n = read(inpipefd[0], body, sizeof(body))) > 0) {
                write(conn, body, n);
        }
        close(inpipefd[0]);

        /* Restore original privileges (same as serve()) */
        setuid(hostuid);
        setgid(hostgid);

        DEBUGF("PPP script finished\n");
        return 0;
}
