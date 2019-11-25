/*
 * main.c
 *
 *  Created on: Nov 22, 2019
 *      Author: sdragoi
 */


#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), connect(), recv() and send() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for files handling */
#include <fcntl.h>		/* for files handling */

#define MAX_CLIENTS		10
#define BUFFER_SIZE		100

/* I am the server */

int main(int argc, char **argv)
{
	char server_buffer[BUFFER_SIZE];
	int server_sock, new_sock;
	int n_bytes, status;
	short port;
	unsigned int addr_len;
	int fd_client, fd_requested, max_client_fd;
	fd_set sock_set, tmp_set;
	struct sockaddr_in client_addr, server_addr;

	if (argc != 2) {
		printf("Usage ./server server_port\n");
		return 0;
	} else {
		sscanf(argv[1], "%hd", &port);
		server_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socket < 0) {
			printf("Error creating server socket\n");
			return 0;
		}

		/* Set server address */
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(port);
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* Bind to the default IP address */

		status = bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
		if (status < 0) {
			printf("Server unable to bind\n");
			return 0;
		}

		status = listen(server_sock, MAX_CLIENTS);
		if (status < 0) {
			printf("Server unable to listen\n");
			return 0;
		}

		FD_ZERO(&sock_set);
		FD_ZERO(&tmp_set);
		FD_SET(server_sock, &sock_set);
		max_client_fd = server_sock;
		addr_len = sizeof(addr_len);

		tmp_set = sock_set;
		while (select(max_client_fd + 1, &tmp_set, NULL, NULL, NULL) > 0) {
			if (FD_ISSET(server_sock, &tmp_set)) { /* A new connection on server socket */
				new_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
				if (new_sock < 0) {
					printf("Error accepting new client\n");
					continue;
				}

				/* Add the new socket to sockets list */
				FD_SET(new_sock, &sock_set);
				if (new_sock > max_client_fd) {
					max_client_fd = new_sock;
				}
			}

			for(fd_client = 0; fd_client <= max_client_fd; fd_client++) {
				if (FD_ISSET(fd_client, &tmp_set) && fd_client != server_sock) {
					n_bytes = recv(fd_client, server_buffer, BUFFER_SIZE, 0);
					if (n_bytes < 0) {
						printf("Error receiving from client %d\n", fd_client);
						close(fd_client);
						FD_CLR(fd_client, &sock_set);
					} else if (n_bytes == 0) {
						printf("Client %d closed connection\n", fd_client);
						close(fd_client);
						FD_CLR(fd_client, &sock_set);
					}
					else {
						printf("Client %d wants file: %s\n", fd_client, server_buffer);
						fd_requested = open(server_buffer, O_RDONLY, 0644);

						if (fd_requested < 0) {
							printf("File not found\n");
							memset(server_buffer, 0, sizeof(server_buffer));
							strncpy(server_buffer, "File not found", strlen("File not found") + 1);
							n_bytes = send(fd_client, server_buffer, strlen(server_buffer) + 1, 0);
							continue;
						}

						memset(server_buffer, 0, sizeof(server_buffer));
						n_bytes = read(fd_requested, &server_buffer, BUFFER_SIZE);
						while(n_bytes > 0) {
							send(fd_client, server_buffer, n_bytes, 0);
							memset(server_buffer, 0, sizeof(server_buffer));
							n_bytes = read(fd_requested, &server_buffer, BUFFER_SIZE);
						}
					}
				}
			}
			tmp_set = sock_set;
		}

		status = close(server_sock);
		if (status < 0) {
			printf("Error closing server socket\n");
		}
	}

	return 0;
}
