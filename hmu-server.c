#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

void ignore_sigpipe(void)
{
	struct sigaction myaction;

	myaction.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &myaction, NULL);
}

ssize_t read_line(int fd, char *buffer, size_t n)
{
	size_t i = 0;
	while (i < n - 1)
	{
		char c;
		ssize_t res = read(fd, &c, 1);
		if (res <= 0)
		{
			return res;
		}
		if (c == '\n')
		{
			break;
		}
		buffer[i] = c;
		i++;
	}
	buffer[i] = '\0';
	return i;
}

void handle_client(int client_fd, unsigned long serial)
{
	char username[9];
	char filename[101];
	long size;

	if (read_line(client_fd, username, sizeof(username)) <= 0)
	{
		perror("Failed to read username");
		return;
	}
	if (read_line(client_fd, filename, sizeof(filename)) <= 0)
	{
		perror("Failed to read filename");
		return;
	}
	char size_str[21];
	if (read_line(client_fd, size_str, sizeof(size_str)) <= 0)
	{
		perror("Failed to read file size");
		return;
	}

	size = atol(size_str);

	char full_filename[120];
	sprintf(full_filename, "%s-%lu-%s", username, serial, filename);
	FILE *f = fopen(full_filename, "wb");

	if (f == NULL)
	{
		perror("can't open file");
		return;
	}

	char buffer[BUFFER_SIZE];
	ssize_t n;
	while (size > 0 && (n = read(client_fd, buffer, BUFFER_SIZE)) > 0)
	{
		size_t written = fwrite(buffer, 1, n, f);
		if (written < n)
		{
			perror("Failed to write all bytes to the file");
			fclose(f);
			remove(full_filename);
			return;
		}
		size = size - n;
	}

	fclose(f);

	if (size > 0)
	{
		remove(full_filename);
	}
	else
	{
		char serial_number[11];
		snprintf(serial_number, sizeof(serial_number), "%lu\n", serial);
		ssize_t written = write(client_fd, serial_number, strlen(serial_number));
		if (written < strlen(serial_number))
		{
			perror("Failed to send serial number to the client");
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "usage: %s server_IP portnum\n", argv[0]);
		exit(1);
	}

	ignore_sigpipe();

	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1)
	{
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in a;
	memset(&a, 0, sizeof(struct sockaddr_in));
	a.sin_family = AF_INET;
	a.sin_port = htons(atoi(argv[1]));
	a.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sfd, (struct sockaddr *)&a, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	if (listen(sfd, 3) < 0)
	{
		perror("listen failed");
		exit(EXIT_FAILURE);
	}

	unsigned long serial = 0;

	for (;;)
	{
		struct sockaddr_in ca;
		socklen_t ca_len = sizeof(ca);
		int cfd = accept(sfd, (struct sockaddr *)&ca, &ca_len);
		if (cfd < 0)
		{
			perror("accept failed");
			continue;
		}

		pid_t pid = fork();
		if (pid < 0)
		{
			perror("fork failed");
			close(cfd);
		}
		else if (pid == 0)
		{
			close(sfd);
			handle_client(cfd, serial);
			exit(0);
		}
		else
		{
			close(cfd);
		}

		serial++;
	}

	close(sfd);
	return 0;
}
