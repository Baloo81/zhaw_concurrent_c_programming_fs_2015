#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>


char host[] = "localhost";
typedef struct
{
	const char *name;
	const char *port;
} config;

struct sock_s
{
	int fd;
	struct addrinfo *addr;
};

typedef struct sock_s sock_t;

struct addrinfo init_hints(int sock_type, int flags)
{
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = sock_type;
	if (flags)
		hints.ai_flags = flags;
	return hints;
}

struct addrinfo *resolve_dns(struct addrinfo *hints, char* host, const char *port)
{
	struct addrinfo *servinfo;

	int err = getaddrinfo(host, port, hints, &servinfo);
	if (err)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		exit(1);
	}
	return servinfo;
}

static sock_t connect_socket_to_address(struct addrinfo *servinfo)
{
	struct addrinfo *p;
	int sockfd;
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd == -1)
		{
			perror("Client: socket");
			continue;
		}

		int err = connect(sockfd, p->ai_addr, p->ai_addrlen);
		if (err)
		{
			close(sockfd);
			perror("Client: connect");
			continue;
		}

		break;
	}
	return (sock_t) { sockfd, p };
}


int sendall(int s, char *buffer, int len)
{
	int total = 0;
	int bytesrest = len;
	int n;

	while (total < len)
	{
		n = send(s, buffer+total, bytesrest, 0);
		if (n == -1)
			break;
		total += n;
		bytesrest -= n;
	}
	return n == -1 ? -1 : 0;
}




void printUsage(char const argv[])
{
	printf("Usage: %s <name> [port] [server]\n", argv);
	printf("- Name ist ein Pflichtparameter\n");
	printf("- Port ist ein optionaler Parameter, der Defaultwert ist: 12345\n");
	printf("- Server ist ein optionaler Parameter, der Defaultwert ist: Localhost\n");
	exit(0);
}

config process_options(int argc, char const *argv[], config client)
{
	if (argc == 2)
	{
		client.name = argv[1];
		if (strcmp(client.name, ""))
			client.port = "12345";
		else
			printUsage(argv[0]);
	}
	else if (argc == 3)
	{
		client.name = argv[1];
		client.port = argv[2];
		if (!(atoi(client.port) && strcmp(client.name, "")))
			printUsage(argv[0]);
	}
	else if (argc == 4)
	{
		client.name = argv[1];
		client.port = argv[2];
		if (!(atoi(client.port) && strcmp(client.name, "")))
			printUsage(argv[0]);	
		strcpy(host,argv[3]);
	}
	else
		printUsage(argv[0]);

	return client;
}

int main(int argc, char const *argv[])
{
	config client;
	client = process_options(argc, argv, client);

	struct addrinfo hints = init_hints(SOCK_STREAM, 0);

	struct addrinfo *servinfo = resolve_dns(&hints, host, client.port);
	sock_t sock = connect_socket_to_address(servinfo);

	if (sock.addr == NULL)
	{
		fprintf(stderr, "Fehler bei der Verbindung\n");
		exit(2);
	}


	freeaddrinfo(servinfo);

	if (sendall(sock.fd, "HELLO\n", 7) == -1)
		perror("sendall");

	char buffer[256];
	int size = 0;

	int nbytes = recv(sock.fd, buffer, 256 - 1, 0);
	if (nbytes < 0)
	{
		perror("recv");
		exit(1);
	}
	buffer[nbytes] = '\0';

	printf("Servermessage %s\n", buffer);

	if (strncmp(buffer, "SIZE", 4) == 0)
	{
		size = atoi(buffer+5);
		memset(buffer, 0, sizeof(buffer));

		while(1)
		{
			nbytes = recv(sock.fd, buffer, 255, 0);
			if (nbytes < 0)
			{
				perror("recv");
				exit(1);
			}
			buffer[nbytes] = '\0';
			if (strcmp(buffer, "START\n") == 0)
				break;
			if (strcmp(buffer, "NACK\n") == 0)
			{
				close(sock.fd);
				exit(0);
			}
		}

		while(1)
		{
			int x,y;
			for (y = 0; y < size; ++y)
			{
				for (x = 0; x < size; ++x)
				{
					memset(buffer, 0, sizeof(buffer));

					char take[255] = "TAKE ";
					char field[32];

					sprintf(field, "%d", x);
					strcat(field, " ");
					strcat(take, field);
					memset(field, 0, sizeof(field));

					sprintf(field, "%d", y);
					strcat(field, " ");
					strcat(take, field);
					memset(field, 0, sizeof(field));

					strcat(take, client.name);
					strcat(take, "\n");

					if (sendall(sock.fd, take, sizeof(take)) == -1)
						perror("sendall");

					do
					{
						nbytes = recv(sock.fd, buffer, 255, 0);
						if (nbytes < 0)
						{
							perror("recv");
							exit(1);
						}
						
					} while(nbytes == 0);
					buffer[nbytes] = '\0';
					printf("Servermessage: %s\n", buffer);
					if (strncmp(buffer, "END", 3) == 0)
					{
						close(sock.fd);
						exit(0);
					}
				}
			}
		}
	}

	return 0;
}