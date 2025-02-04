#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

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

void	fatal_error()
{
	char *err = "Fatal Error\n";
	write(2, err, strlen(err));
	exit(1);
}

typedef struct s_client{
	int id;
	int fd;
	struct s_client *next;
} 				t_client;

t_client *clients = NULL;
int		client_id = 0;
fd_set	tmp_fds, read_fds, write_fds;
int sockfd, connfd, len;

char	notice[1024];
char	buf[1024];
char	tmp[1024];

// loop thru all clients returning highest fd
int	get_max_fds()
{
	t_client *tmp = clients;
	int max = 0;
	while (tmp)
	{
		if (tmp->fd > max)
			max = tmp->fd;
		tmp = tmp->next;
	}
	return max;
}

void	send_to_all(int fd, char *notice)
{
	t_client *current = clients;

	printf("sending to all clients");

	while (current)
	{
		if (FD_ISSET(current->fd, &write_fds), current->fd != fd)
		{
			if (send(current->fd, notice, strlen(notice), 0) < 0)
				fatal_error();
		}
		current = current->next;
	}
}

void	accept_client()
{
	struct sockaddr_in	client_address;
	socklen_t 			size = sizeof(client_address);
	int					fd;
	t_client 			*new = NULL;

	if ((fd = accept(sockfd, (struct sockaddr *)&client_address, &size)) < 0)
		fatal_error();
	bzero(&notice, strlen(notice));
	sprintf(notice, "server: client %d just arrived", client_id);
	send_to_all(fd, notice);
	FD_SET(fd, &tmp_fds);

	new = calloc(1, sizeof(t_client));
	if (!new)
		fatal_error;
	
	new->id = client_id;
	new->fd = fd;
	new->next = NULL;
	if (!clients)
	{
		printf("first connection");
		clients = new;
	}
	else
	{
		printf("adding new client");
		t_client *current = clients;
		while(current->next)
			current = current->next;
		current->next = new;
	}
	client_id++;
}

int	get_client_id(int fd)
{
	t_client *current = clients;

	while (current)
	{
		if (current->fd == fd)
			return (current->fd);
		current = current->next;
	}
	return (-1);
}

void	remove_client(fd)
{
	t_client *current = clients;
	t_client *to_rm;

	int id = get_client_id(fd);
	printf("removing client id [%d]", id);

	if (current && current->fd == fd)
	{
		clients = current->next;
		free(current);
	}
	else
	{
		while (current && current->next && current->next->fd != fd)
			current = current->next;
		to_rm = current->next;
		free(to_rm);
	}
	bzero(&notice, strlen(notice));
	sprintf(notice, "server: client %d just left", id);
	send_to_all(fd, notice);
	FD_CLR(fd, &tmp_fds);
	close(fd);
}

void	msg_from_client(fd)
{
	int i = 0;
	int j = 0;

	bzero(&tmp, strlen(tmp));
	while (buf[i])
	{
		tmp[j] = buf[i];
		j++;
		//might not be the end of the message
		if (buf[i] == '\n')
		{
			sprintf(notice, "client %d: %s", get_client_id(fd), tmp);
			//printf("notice : %s\n", notice);
			send_to_all(fd, notice);
			j = 0;
			bzero(&buf, strlen(buf));
			bzero(&notice, strlen(notice));
		}
		i++;
	}
	bzero(&buf, strlen(buf));

}

/* program
	- av[1] = port to bind to
	- syscall error/malloc > "Fatal Error\n"
	- when client connects:
		* assign client id from 0
		* send to all clients "server: client %d just arrived\n"accept
	- clients can send messages to server:
		* message can have multiple '\n'
		* when server receives msg "client %d: "(msg)
	- when client disconnects:
		* to all clients "server: client %d just left\n"
 */
int main(int ac, char **av) {
	if (ac != 2)
		fatal_error();
	
	int	port = atoi(av[1]);
	struct sockaddr_in servaddr, cli; 

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		fatal_error();
	else
		printf("Socket [%d] successfully created..\n", sockfd); 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal_error();
	else
		printf("Socket successfully binded..\n");
	
	if (listen(sockfd, 0))
		fatal_error();	

	FD_ZERO(&tmp_fds);
	FD_SET(sockfd, &tmp_fds);
	bzero(&notice, strlen(notice));
	bzero(&buf, strlen(buf));

	while (1)
	{
		tmp_fds = read_fds;
		read_fds = write_fds;
		int max_fds = get_max_fds() + 1;

		if (select(max_fds, &read_fds, &write_fds, NULL, NULL) == -1)
			continue;

		//loop thru all clients
		for (int fd = 0; fd <= max_fds; fd++)
		{

			//check if fd is in fd_set
			if (!FD_ISSET(fd, &read_fds))
				continue;
			//new client
			if (fd == sockfd)
			{
				printf("Accepting client [%d]", fd);
				accept_client();
				break ;
			}


			//receive msg from client
			int ret = 1;
			while (ret == 1 && buf[strlen(buf) - 1] != '\n')
			{
				ret = recv(fd, buf + strlen(buf), 1, 0);
				if (ret <= 0)
					break ;
			}

			//if recv() returns -1 or 0 bytes, remove client 
			if (ret <= 0)
			{
				printf("removing client [%d]", fd);
				remove_client(fd);
				break ;
			}
			//msg from client
			else
			{
				msg_from_client(fd);
				break ;
			}
		}
	}
	return 0;
}