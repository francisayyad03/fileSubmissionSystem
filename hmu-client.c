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

int main(int argc, char *argv[])
{
	if (argc != 5)
	{
		fprintf(stderr, "Need server IPv4 address, port, username, and filename.\n");
		exit(EXIT_FAILURE);
	}

	if (strlen(argv[3]) < 1 || strlen(argv[3]) > 8 || strlen(argv[4]) < 1 || strlen(argv[4]) > 100)
	{
		fprintf(stderr, "Invalid username or filename.\n");
		exit(EXIT_FAILURE);
	}

	int cfd = socket(AF_INET, SOCK_STREAM, 0);
	if (cfd == -1)
	{
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in a;
	memset(&a, 0, sizeof(struct sockaddr_in));
	a.sin_family = AF_INET;
	a.sin_port = htons(atoi(argv[2]));
	if (inet_pton(AF_INET, argv[1], &a.sin_addr) <= 0)
	{
		perror("Invalid IPv4 address");
		exit(EXIT_FAILURE);
	}

	if (connect(cfd, (struct sockaddr *)&a, sizeof(struct sockaddr_in)) < 0)
	{
		perror("Connection failed");
		exit(EXIT_FAILURE);
	}

	FILE *f = fopen(argv[4], "rb");
	if (f == NULL)
	{
		perror("Can't open file");
		exit(EXIT_FAILURE);
	}

	fseek(f, 0L, SEEK_END);
	long size = ftell(f);
	rewind(f);

	FILE *sockfile = fdopen(cfd, "w");
	if (sockfile == NULL)
	{
		perror("Failed to open socket as file");
		exit(EXIT_FAILURE);
	}

	fprintf(sockfile, "%s\n%s\n%ld\n", argv[3], argv[4], size);
	fflush(sockfile);

	char buffer[BUFFER_SIZE];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0)
	{
		ssize_t written = write(cfd, buffer, n);
		if (written < n)
		{
			perror("Failed to write all bytes to the socket");
			exit(EXIT_FAILURE);
		}
	}

	fclose(f);

	char serial_number[11];
	ssize_t len = read(cfd, serial_number, sizeof(serial_number) - 1);
	if (len <= 0)
	{
		perror("Failed to read serial number from the server");
		exit(EXIT_FAILURE);
	}

	serial_number[len] = '\0';

	if (serial_number[len - 1] != '\n')
	{
		fprintf(stderr, "Invalid serial number received from server.\n");
		exit(EXIT_FAILURE);
	}

	printf("%d\n", atoi(serial_number));

	close(cfd);

	return 0;
}