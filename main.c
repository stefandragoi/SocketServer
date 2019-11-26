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
#define BAD_FILE_MSG			"Bad file name"
#define FILE_NOT_FOUND_MSG		"File not found"
#define SERVER_ERROR_MSG		"Server reading error"

int wildcard_check(char string[]) {
	int i, len;
	len = strlen(string);

	for (i = 0; i < len; i++) {
		switch(string[i]) {
		case '?':
		case '*':
		case '[':
		case ']':
		case '^':
			return 0;
		default:
			continue;
		}
	}

	return 1;
}

short get_port(char string[]) {
	short port;
	int i, len, status;

	len = strlen(string);
	for (i = 0; i < len; i++) {
		if (string[i] < '0' || string[i] > '9') {
			printf("Wrong port format\n");
			return -1;
		}
	}
	status = sscanf(string, "%hd", &port);
	if (status == EOF) {
		printf("sscanf port error\n");
		return -1;
	}

	return port;
}

void process_client(int fd_client, fd_set *sock_set) {
	int n_bytes_read, n_bytes_sent;
	int fd_requested;
	char buffer[BUFFER_SIZE];

	memset(buffer, 0, sizeof(buffer));
	n_bytes_read = recv(fd_client, buffer, BUFFER_SIZE, 0);
	if (n_bytes_read < 0) {
		printf("Error receiving from client %d\n", fd_client);
		close(fd_client);
		FD_CLR(fd_client, sock_set);
	} else if (n_bytes_read == 0) {
		printf("Client %d closed connection\n", fd_client);
		close(fd_client);
		FD_CLR(fd_client, sock_set);
	} else {
		/* Check for forbidden characters */
		if (0 == wildcard_check(buffer)) {
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, BAD_FILE_MSG, strlen(BAD_FILE_MSG));
			n_bytes_sent = send(fd_client, buffer, strlen(BAD_FILE_MSG) + 1, 0);
			return;
		}

		fd_requested = open(buffer, O_RDONLY, 0600);
		if (fd_requested < 0) {
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, FILE_NOT_FOUND_MSG, strlen(FILE_NOT_FOUND_MSG));
			n_bytes_sent = send(fd_client, buffer, strlen(FILE_NOT_FOUND_MSG) + 1, 0);
			return;
		}

		memset(buffer, 0, sizeof(buffer));
		/* Read from requested file maximum BUFFER_SIZE bytes until the file ends */
		n_bytes_read = read(fd_requested, &buffer, BUFFER_SIZE);
		if (n_bytes_read < 0) {
			printf("Error reading requested file\n");
			strncpy(buffer, SERVER_ERROR_MSG, strlen(SERVER_ERROR_MSG));
			n_bytes_sent = send(fd_client, buffer, strlen(SERVER_ERROR_MSG) + 1, 0);
			return;
		}
		while(n_bytes_read > 0) {
			n_bytes_sent = send(fd_client, buffer, n_bytes_read, 0);
			if (n_bytes_sent < 0) {
				printf("Error sending file to client %d\n", fd_client);
				continue;
			}
			if (n_bytes_sent < n_bytes_read) { /* Client buffer may be smaller */
				while(n_bytes_sent != n_bytes_read) {
					n_bytes_sent += send(fd_client, buffer + n_bytes_sent, n_bytes_read - n_bytes_sent, 0);
				}
			}
			memset(buffer, 0, sizeof(buffer));
			n_bytes_read = read(fd_requested, &buffer, BUFFER_SIZE);
		}
	}
}

int main(int argc, char **argv)
{
	int server_sock, new_sock, fd_client, max_client_fd;
	int status;
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
		port = get_port(argv[1]);
		if (port < 0) {
			return 0;
		}

		/* Create server socket */
		server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (server_sock < 0) {
			printf("Error creating server socket\n");
			return 0;
		}

		/* Bind to the default IP address */
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(port);
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
		/* Use a copy of socket set for select function */
		FD_ZERO(&tmp_set);
		/* Monitor server socket for new connections */
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
				if (FD_ISSET(server_sock, &tmp_set)) {
					/* A new connection on server socket */
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

				/* Check if a client sends something */
				for(fd_client = 0; fd_client <= max_client_fd; fd_client++) {
					if (FD_ISSET(fd_client, &tmp_set) && fd_client != server_sock) {
						process_client(fd_client, &sock_set);
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
