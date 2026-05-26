#include "defs.h"
#include "response.h"
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int server;
char root[MAX_PATH_SIZE];
char debug = 0;
uid_t hostuid;
gid_t hostgid;

volatile sig_atomic_t active_conns = 0;

void sig_handler() {
        puts("closing server");
        close(server);
        exit(1);
}

void sigchld_handler(int sig) {
        int saved_errno = errno;
        while (waitpid(-1, NULL, WNOHANG) > 0) {
                if (active_conns > 0)
                        active_conns--;
        }
        errno = saved_errno;
}

int main(int argc, char *argv[]) {
	if (getenv("DEBUG")) {
                debug = 1;
        }

        int portnum = 8080;

        char *port = getenv("PORT");
        if (port != NULL) {
                char *endptr = NULL;
                long p = strtol(port, &endptr, 10);
                if (*endptr == '\0' && p > 0 && p <= 65535) {
                        portnum = (int)p;
                } else {
                        fprintf(stderr, "Invalid PORT value '%s', falling back to 8080\n", port);
                }
        }

        for (int i = 1; i < argc; i++) {
                if (argv[i][1] == 'r') {
                        strcpy(root, argv[i + 1]);
                        printf("set root dir to %s\n", root);
                }
        }

        signal(SIGINT, sig_handler);
        signal(SIGKILL, sig_handler);
        signal(SIGSTOP, sig_handler);
        signal(SIGCHLD, sigchld_handler);

	hostuid = getuid();
	hostgid = getgid();

        /* Initiate a TCP socket */
        server = socket(AF_INET, SOCK_STREAM, 0);
        if (server < 0) {
                perror("socket creation failed");
                return 0;
        }

        /* Make server reuse addresses */
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

        /* The server attributes */
        struct sockaddr_in serv, client;
        serv.sin_family = AF_INET;
        serv.sin_port = htons(portnum);
        serv.sin_addr.s_addr = INADDR_ANY;

        /* Bind the server to the address */
        if (bind(server, (struct sockaddr *)&serv, 16) < 0) {
                perror("bind failed");
                close(server);
                return 0;
        }

	DEBUGF("running in debug mode\n");
        printf("server is listening on port %d\n", portnum);

        /* Now we listen */
        listen(server, 10);
        socklen_t cli_len;
        int conn;

        while (1) {
                /* Get the new connection */
                cli_len = sizeof(client);
                conn = accept(server, (struct sockaddr *)&client, &cli_len);
		DEBUGF("got a connection from %s\n", inet_ntoa(client.sin_addr));

                /*  We got a connection! Check if it is OK */
                if (conn < 0) {
                        perror("couldn't connect");
                        continue;
                }

                /* Global concurrent connection limit */
                if (active_conns >= MAX_CONCURRENT_CONNECTIONS) {
                        close(conn);
                        continue;
                }
                active_conns++;

                /* Fork the connection */
                pid_t conn_pid = fork();

                /* Manipulate those processes */
                if (conn_pid < 0) {
                        perror("couldn't fork");
                        active_conns--;   /* fork failed, roll back counter */
                        close(conn);
                        continue;
                }

                if (conn_pid == 0) {
                        /* This is the child process */
                        /* Close the parent connection */
                        close(server);

                        /* Process the response */
                        handle_request(conn);

                        /* Close the client socket, not needed anymore */
                        close(conn);
                        exit(0);
                } else {
                        /* This is the master process */
                        /* Close the connection */
                        close(conn);
                }
        }

        return 0;
}
