#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>


int client_id = 0;
int	max_fd = 0;
int clients[SOMAXCONN];
char *msgs[SOMAXCONN];

fd_set rfds, wfds, tfds; //read, write, temp

char buf_read[2048], buf_write[2048];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	fatal(char *err)
{
	write(2, err, strlen(err));
	exit(1);
}

void	send_to_all(int sender, char *str)
{
	for (int fd = 0; fd <= max_fd; fd++)
	{
		if (FD_ISSET(fd, &wfds) && fd != sender) //sockfd (3) should not be writable, but maybe need a check for fd != sockfd
		{
			int ret = 0;
			if ((ret = send(fd, str, strlen(str), MSG_NOSIGNAL)) <= 0) 	//MSG_NOSIGNAL makes send return -1 instead of broken pipe when message fails to send
			{
				// fatal("Fatal error\n"); //unsure if this should be fatal error or just continue program
			} 
		}
	}
}

void	add_client(int cfd)
{
	if (cfd > max_fd)
		max_fd = cfd;
	clients[cfd] = client_id++;
	msgs[cfd] = NULL;

	FD_SET(cfd, &tfds);
	sprintf(buf_write, "server: client %d just arrived\n", clients[cfd]);
	send_to_all(cfd, buf_write);
}

void rm_client(int cfd)
{
	sprintf(buf_write, "server: client %d just left\n", clients[cfd]);

	// printf("rm %d -- remaining clients:\n", cfd);
	// for (int i = 3; i <= max_fd; i++)
	// 	printf("\tclientid: %d fd %d\n", clients[i], i);
	send_to_all(cfd, buf_write);

	free(msgs[cfd]);
	FD_CLR(cfd, &tfds);
	close(cfd);
}


void	send_msg(int sender)
{
	char *msg;

	while(extract_message(&msgs[sender], &msg)) //extract message allocates msg
	{
		sprintf(buf_write, "client %d: ", clients[sender]);
		send_to_all(sender, buf_write);
		send_to_all(sender, msg);
		free(msg);
	}
}

int	init_serv(int port)
{
	int sockfd = 0;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		printf("socket creation failed...\n"); 
		exit(0); 
	} 
	else
		printf("Socket successfully created..\n"); 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		printf("socket bind failed...\n"); 
		exit(0); 
	} 
	else
		printf("Socket successfully binded..\n");
	if (listen(sockfd, SOMAXCONN) != 0) {
		printf("cannot listen\n"); 
		exit(0); 
	}
	max_fd = sockfd;
	return (sockfd);
}

int main(int ac, char **av)
{
	if (ac != 2)
		fatal("Wrong number of arguments\n");

	int port = atoi(av[1]);
	// socket create and verification 
	int sockfd = init_serv(port);

	FD_ZERO(&tfds);
	FD_SET(sockfd, &tfds);
	while (1)
	{
		rfds = wfds = tfds; //copy all fds into each other

		if (select(max_fd + 1, &rfds, &wfds, NULL, NULL) < 0)
			fatal("select\n"); //or continue?

		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (!FD_ISSET(fd, &rfds))
				continue;

			if (fd == sockfd)
			{
				struct sockaddr_in cli;
				unsigned int len = sizeof(cli);

				int cfd = accept(sockfd, (struct sockaddr *)&cli, &len);
				if (cfd < 0)
					fatal("accept\n");
				else
				{
					add_client(cfd);
					break ; //new client added, need to loop thru fds again
				}
			}
			else
			{
				int read_bytes = recv(fd, buf_read, 2048, 0);
				if (read_bytes <= 0)
				{
					rm_client(fd);
					break ;
				}
				buf_read[read_bytes] = '\0';
				msgs[fd] = str_join(msgs[fd], buf_read);
				send_msg(fd);
			}
		}
	}
	return 0;
}