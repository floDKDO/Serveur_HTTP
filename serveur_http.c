#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>


#define CHK(op) do { if ((op) == -1) {perror(""); exit(EXIT_FAILURE);} } while (0)
#define NCHK(op) do { if ((op) == NULL) {perror(""); exit(EXIT_FAILURE);} } while (0)


#define MAX_BACKLOG_LENGTH 10
#define MAX_LENGTH_BUFFER 1518
#define PORT_NUMBER 8888


struct mes_clients //le premier élément de cette struct contient la socket d'écoute du serveur
{
	int sock_fd;
	struct sockaddr_in sa_client;
};



void affiche_info_client(struct mes_clients client)
{
	printf("Informations sur le client : \n");
	printf("- numéro de descripteur de la socket : %d\n", client.sock_fd);
	char ip_address[INET_ADDRSTRLEN];
	NCHK(inet_ntop(AF_INET, &client.sa_client.sin_addr, ip_address, INET_ADDRSTRLEN));
	printf("- adresse IP du client : %s\n", ip_address);
	printf("- port du client : %d\n", htons(client.sa_client.sin_port));
}


int ajout_client(int sock_fd, struct sockaddr_in sa_client, struct mes_clients* clients)
{
	for(int i = 0; i < MAX_BACKLOG_LENGTH; ++i)
	{
		if(clients[i].sock_fd == -1)
		{
			clients[i].sock_fd = sock_fd;
			clients[i].sa_client = sa_client;
			return i;
		}
	}
	return -1;
}


int get_max_sock_fd(struct mes_clients* clients)
{
	int sock_fd = -1;
	for(int i = 0; i < MAX_BACKLOG_LENGTH; ++i)
	{
		if(clients[i].sock_fd > sock_fd)
		{
			sock_fd = clients[i].sock_fd;
		}
	}
	return sock_fd;
}


int creer_socket_serveur(void)
{
	int s_ecoute;
	CHK(s_ecoute = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP));
	
	int x = 1;
 	CHK(setsockopt(s_ecoute, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x)));
 	
 	return s_ecoute;
}


struct sockaddr_in serveur_bind_listen(int s_ecoute)
{
	struct sockaddr_in serveur;
 	serveur.sin_family = AF_INET;
 	serveur.sin_port = htons(PORT_NUMBER);
 	serveur.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
 	
 	socklen_t len_serveur = sizeof(serveur);
 	
 	CHK(bind(s_ecoute, (struct sockaddr*)&serveur, len_serveur));
 	
 	printf("En écoute sur le port %d...\n", PORT_NUMBER);
 	
 	CHK(listen(s_ecoute, MAX_BACKLOG_LENGTH));
 	
 	return serveur;
}


void reset_fd_set(fd_set* set, struct mes_clients* clients)
{
	FD_ZERO(set);
	for(int i = 0; i < MAX_BACKLOG_LENGTH; ++i) //repositionner les descripteurs qui ont été modifiés au retour de select()
	{
		if(clients[i].sock_fd != -1)
			FD_SET(clients[i].sock_fd, set);
	}
}


void arrivee_nouveau_client(int s_ecoute, struct mes_clients* clients)
{
	socklen_t len_client = sizeof(struct sockaddr_in);
	struct sockaddr_in sa_client;

 	int s;
 	CHK(s = accept(s_ecoute, (struct sockaddr*)&sa_client, &len_client));
 	int index_client = ajout_client(s, sa_client, clients);
 	
 	affiche_info_client(clients[index_client]);
}


void get_date_GMT(char* date_buffer, size_t length)
{
	time_t t;
	if((t = time(NULL)) == (time_t) -1)
	{
		perror("time");
		exit(EXIT_FAILURE);
	}
	
	struct tm* tm; 
	NCHK(tm = gmtime(&t)); //GMT
	
	strftime(date_buffer, length * sizeof(char), "%a, %d %b %Y %T %Z", tm);
}


char* get_contenu_fichier(char* pathname)
{
	FILE* html_file;
	NCHK(html_file = fopen(pathname, "r"));
	
	struct stat statbuf;
	CHK(stat(pathname, &statbuf));
	
	char* buf;
	NCHK(buf = calloc(statbuf.st_size + 1, sizeof(char)));
		
	size_t number_read = fread(buf, sizeof(char), statbuf.st_size + 1, html_file);
	if(feof(html_file))
	{
		buf[number_read] = '\0';
	}
	else if(ferror(html_file)) 
	{
		fprintf(stderr, "Error: ferror()\n");
		exit(EXIT_FAILURE);
	} 
	
	if(fclose(html_file) == EOF)
	{
		perror("fclose");
		exit(EXIT_FAILURE);
	}
	
	return buf;
}


void send_reponse_http(char* method, char* pathname, int status_code, int sock_fd)
{
	char* status_message;
	if(status_code == 200)
	{
		status_message = "OK";
	}
	else if(status_code == 404)
	{
		status_message = "Not Found";
	}
	else if(status_code == 405)
	{
		status_message = "Method Not Allowed";
	}
		
	char date_buffer[100];
	get_date_GMT(date_buffer, 100);
	
	char response[MAX_LENGTH_BUFFER] = {0};
	snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s %d %s %s", "HTTP/1.1", status_code, status_message, "\r\n");
	snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "Server: mon_serveur\r\n");
	snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s%s%s", "Date: ", date_buffer, "\r\n");
	
	char* buf;
	if(strcmp(method, "GET") == 0)
	{
		buf = get_contenu_fichier(pathname);
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s%ld%s", "Content-Length: ", strlen(buf), "\r\n"); 
	}
	else if(strcmp(method, "HEAD") != 0 && strcmp(method, "POST") != 0)
	{
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "Allow: GET, HEAD\r\n");
	}
	
	snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "Content-Type: text/html\r\n");
	snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "\r\n");
	
	if(strcmp(method, "GET") == 0)
	{
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s\r\n", buf);
		free(buf);
	}
	else snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "\r\n");
	
	CHK(send(sock_fd, response, strlen(response), 0));
}


void event_client_existant(struct mes_clients* client)
{
	char buffer[MAX_LENGTH_BUFFER];

	ssize_t recv_count;
	if((recv_count = recv(client->sock_fd, buffer, MAX_LENGTH_BUFFER, 0)) == 0)
	{
		//ici, le client qui possède la socket client.sock_fd s'est terminée 
		CHK(close(client->sock_fd)); 
		client->sock_fd = -1;
		memset(&client->sa_client, 0, sizeof(struct sockaddr_in));
		return; //passer à l'itération suivante car le client n'a rien envoyé vu qu'il s'est terminé
	}
	buffer[recv_count] = '\0';

	char* method;
	char* request_target;
	char buffer_copy[MAX_LENGTH_BUFFER];
	snprintf(buffer_copy, sizeof(buffer_copy), "%s", buffer);

	char* token = strtok(buffer_copy, " \n");

	//split les deux premiers éléments de la première ligne
	int index = 0;
	while(index != 2)
	{
		if(index == 0)
		{
			method = token;
		}
		else if(index == 1)
		{
			request_target = token;
		}
		
		token = strtok(NULL, " \n");
		index += 1;
	}

	if(strcmp(method, "GET") == 0)
	{
		char pathname[1024] = {0};
		snprintf(pathname, sizeof(pathname), "%s", ".");
		snprintf(pathname + strlen(pathname), sizeof(pathname) - strlen(pathname), "%s", request_target);
		
		int status_code;
		
		if(access(pathname, F_OK) == -1)
		{
			status_code = 404;
			printf("The file <%s> is unknown!\n", pathname);
			snprintf(pathname, sizeof(pathname), "%s", "erreur_404.html");
		}
		else status_code = 200;
		
		send_reponse_http(method, pathname, status_code, client->sock_fd);
	}
	else if(strcmp(method, "HEAD") == 0)
	{
		send_reponse_http(method, NULL, 200, client->sock_fd);
	}
	else if(strcmp(method, "POST") == 0)
	{
		char* data = strstr(buffer, "\r\n\r\n");
		
		if(data == NULL)
		{
			printf("Substring non trouvée...\n");
		} 
		else printf("Data du post : %s\n", data + 4); // +1 pour \r, +1 pour \n, +1 pour \r, +1 pour \n => +4
	}
	else
	{
		char pathname[1024];
		snprintf(pathname, sizeof(pathname), "%s", "erreur_405.html");
		send_reponse_http(method, pathname, 405, client->sock_fd);
	}
}


int main(void)
{
	int s_ecoute = creer_socket_serveur();
	
	struct sockaddr_in serveur = serveur_bind_listen(s_ecoute);
 	
 	struct mes_clients clients[MAX_BACKLOG_LENGTH];
 	for(int i = 0; i < MAX_BACKLOG_LENGTH; ++i)
 	{
 		clients[i].sock_fd = -1;
 		memset(&clients[i].sa_client, 0, sizeof(struct sockaddr_in));
 	}
 	
 	clients[0].sock_fd = s_ecoute;
 	clients[0].sa_client = serveur;
 	
 	fd_set set;
 	FD_ZERO(&set);
 	FD_SET(clients[0].sock_fd, &set);
 	
 	while(1)
 	{
 		reset_fd_set(&set, clients);
 		CHK(select(get_max_sock_fd(clients) + 1, &set, NULL, NULL, NULL));
 	
 		for(int i = 0; i < MAX_BACKLOG_LENGTH; ++i)
 		{
 			if(FD_ISSET(clients[i].sock_fd, &set))
 			{
 				if(clients[i].sock_fd == s_ecoute) //serveur => arrivée d'un nouveau client
 				{
 					arrivee_nouveau_client(s_ecoute, clients);
 				}
 				else //client qu'on connaît déjà / qui était déjà là
 				{
 					event_client_existant(&clients[i]);
 				}
 			}
 		}
 	}
	return 0;
}
