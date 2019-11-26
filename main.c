/*
 * main.c
 *
 *  Created on: Nov 22, 2019
 *      Author: sdragoi
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_CLIENTS				10
#define BUFFER_SIZE				1000
#define TIMEOUT_SEC				1
#define TIMEOUT_USEC			0
#define FILE_NOT_FOUND_MSG		"File not found"

int wildcard_check(char string[]) {
	int i, len;
	len = strlen(string);

	for (i = 0; i < len; i++) {
		switch(string[i]) {
		case '?':
		case '*':
		case '[':
		case ']':
			return 0;
		default:
			continue;
		}
	}

	return 1;
}

int main(int argc, char **argv)
{
	char server_buffer[BUFFER_SIZE];
	int server_sock, new_sock, fd_client, fd_requested, max_client_fd;
	int i, n_bytes_read, n_bytes_sent, status;
	short port;
	unsigned int addr_len;

	fd_set sock_set, tmp_set;
	struct sockaddr_in client_addr, server_addr;
	struct timeval timeout;

	timeout.tv_sec = TIMEOUT_SEC;
	timeout.tv_usec = TIMEOUT_USEC;

	if (argc != 2) {
		printf("Usage ./server server_port\n");
		return 0;
	} else {
		/* check port format */
		for (i = 0; i < strlen(argv[1]); i++) {
			if (argv[1][i] < '0' || argv[1][i] > '9') {
				printf("Wrong port format\n");
				return 0;
			}
		}
		status = sscanf(argv[1], "%hd", &port);
		if (status == EOF) {
			printf("Sscanf port error\n");
			return 0;
		}

		server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socket < 0) {
			printf("Error creating server socket\n");
			return 0;
		}

		/* Set server address */
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(port);
		/* Bind to the default IP address */
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

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

		while (1) {
			tmp_set = sock_set;
			status = select(max_client_fd + 1, &tmp_set, NULL, NULL, &timeout);
			if (status < 0) {
				printf("Error in select function\n");
				break;
			} else if (status == 0) { /* No pending connection */
				continue;
			} else {
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

				/* Check every client */
				for(fd_client = 0; fd_client <= max_client_fd; fd_client++) {
					if (FD_ISSET(fd_client, &tmp_set) && fd_client != server_sock) {
						n_bytes_read = recv(fd_client, server_buffer, BUFFER_SIZE, 0);
						if (n_bytes_read < 0) {
							printf("Error receiving from client %d\n", fd_client);
							close(fd_client);
							FD_CLR(fd_client, &sock_set);
						} else if (n_bytes_read == 0) {
							printf("Client %d closed connection\n", fd_client);
							close(fd_client);
							FD_CLR(fd_client, &sock_set);
						} else {
							printf("Client %d wants file: %s\n", fd_client, server_buffer);
							if (0 == wildcard_check(server_buffer)) {
								printf("Forbidden character detected in file name\n");
								continue;
							}
							fd_requested = open(server_buffer, O_RDONLY, 0600);

							if (fd_requested < 0) {
								memset(server_buffer, 0, sizeof(server_buffer));
								strncpy(server_buffer, FILE_NOT_FOUND_MSG, strlen(FILE_NOT_FOUND_MSG));
								n_bytes_sent = send(fd_client, server_buffer, strlen(FILE_NOT_FOUND_MSG), 0);
								continue;
							}

							memset(server_buffer, 0, sizeof(server_buffer));
							/* Read from requested file maximum BUFFER_SIZE bytes until the file ends */
							n_bytes_read = read(fd_requested, &server_buffer, BUFFER_SIZE);
							if (n_bytes_read < 0) {
								printf("Error reading requested file\n");
								memset(server_buffer, 0, sizeof(server_buffer));
								strncpy(server_buffer, FILE_NOT_FOUND_MSG, strlen(FILE_NOT_FOUND_MSG));
								n_bytes_sent = send(fd_client, server_buffer, strlen(FILE_NOT_FOUND_MSG), 0);
								continue;
							}
							while(n_bytes_read > 0) {
								n_bytes_sent = send(fd_client, server_buffer, n_bytes_read, 0);
								if (n_bytes_sent < 0) {
									printf("Error sending file to client %d\n", fd_client);
									continue;
								}
								if (n_bytes_sent < n_bytes_read) { /* Client buffer may be smaller */
									while(n_bytes_sent != n_bytes_read) {
										n_bytes_sent += send(fd_client, server_buffer + n_bytes_sent, n_bytes_read - n_bytes_sent, 0);
									}
								}
								memset(server_buffer, 0, sizeof(server_buffer));
								n_bytes_read = read(fd_requested, &server_buffer, BUFFER_SIZE);
							}
						}
					}
				}
				tmp_set = sock_set;
			}
		}

		status = close(server_sock);
		if (status < 0) {
			printf("Error closing server socket\n");
		}
	}

	return 0;
}
