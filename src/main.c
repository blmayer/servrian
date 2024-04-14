#include "defs.h"
#include "response.h"
#include <arpa/inet.h>
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

void sig_handler() {
        puts("closing server");
        close(server);
        exit(1);
}

int main(int argc, char *argv[]) {
	if (getenv("DEBUG")) {
                debug = 1;
        }

        int portnum = 8080;

        char *port = getenv("PORT");
        if (port != NULL) {
                portnum = atoi(port);
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
        signal(SIGCHLD, SIG_IGN);

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

                /* Fork the connection */
                pid_t conn_pid = fork();

                /* Manipulate those processes */
                if (conn_pid < 0) {
                        perror("couldn't fork");
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
