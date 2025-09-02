#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>


#define CHK(op) do { if ((op) == -1) {perror(""); exit(EXIT_FAILURE);} } while (0)
#define NCHK(op) do { if ((op) == NULL) {perror(""); exit(EXIT_FAILURE);} } while (0)


#define MAX_BACKLOG_LENGTH 1
#define MAX_LENGTH_BUFFER 1518
#define PORT_NUMBER 8888


int main(void)
{
	int s_listen;
	CHK(s_listen = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP));
	
	int x = 1;
 	CHK(setsockopt(s_listen, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x)));
 	
 	struct sockaddr_in serveur;
 	serveur.sin_family = AF_INET;
 	serveur.sin_port = htons(PORT_NUMBER);
 	serveur.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
 	
 	socklen_t len_serveur = sizeof(serveur);
 	
 	CHK(bind(s_listen, (struct sockaddr*)&serveur, len_serveur));
 	
 	CHK(listen(s_listen, MAX_BACKLOG_LENGTH));
 	
 	struct sockaddr_in client;
 	socklen_t len_client = sizeof(client);
 	
 	int s;
 	CHK(s = accept(s_listen, (struct sockaddr*)&client, &len_client));
 	
 	char buffer[MAX_LENGTH_BUFFER];
 	CHK(recv(s, buffer, MAX_LENGTH_BUFFER, 0));
 	
 	char* method;
 	char* request_target;
 	char* protocol;
	
	char* token = strtok(buffer, " \n");
	
	//split la première ligne
	int i = 0;
	while(i != 3)
	{
		if(i == 0)
		{
			method = token;
		}
		else if(i == 1)
		{
			request_target = token;
		}
		else if(i == 2)
		{
			protocol = token;
		}
		
		token = strtok(NULL, " \n");
		i += 1;
	}
	
	char* request_target_prefixed;
	NCHK(request_target_prefixed = calloc(strlen(request_target) + 2, sizeof(char))); // + 2 => +1 pour '.', +1 pour '\0'

	strcat(request_target_prefixed, ".");
	strcat(request_target_prefixed, request_target);
	
	if(strcmp(method, "GET") == 0)
	{
		//char* protocol_response;
		//char* status_code;
		//char* status_text;
		
		FILE* html_file;
		NCHK(html_file = fopen(request_target_prefixed, "r"));
		
		struct stat statbuf;
		CHK(stat(request_target_prefixed, &statbuf));
		
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
		
		char response[MAX_LENGTH_BUFFER] = {0};
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "HTTP/1.1 200 OK \r\n");
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "Server: mon_serveur\r\n");
		size_t size = (statbuf.st_size + 1) * sizeof(char);
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s%ld%s", "Content-Length: ", size,  "\r\n"); 
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "Content-Type: text/html\r\n");
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "\r\n");
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s\r\n", buf);
		free(buf);
		
		CHK(send(s, response, strlen(response), 0));
	}
	else if(strcmp(method, "HEAD") == 0)
	{
		time_t t;
		if((t = time(NULL)) == (time_t) -1)
		{
			perror("time");
			exit(EXIT_FAILURE);
		}
		
		struct tm* tm; 
		NCHK(tm = gmtime(&t)); //GMT
		
		char date[100];
		strftime(date, sizeof(date), "%a, %d %b %Y %T %Z", tm);
	
		char response[MAX_LENGTH_BUFFER] = {0};
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "HTTP/1.1 200 OK \r\n");
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "Server: mon_serveur\r\n");
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s%s%s", "Date: ", date, "\r\n");
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "Content-Type: text/html\r\n");
		snprintf(response + strlen(response), MAX_LENGTH_BUFFER - strlen(response), "%s", "\r\n\r\n");
		
		CHK(send(s, response, strlen(response), 0));
	}
	else if(strcmp(method, "POST") == 0)
	{
	
	}
	else
	{
	
	}
	
	free(request_target_prefixed);
	
	return 0;
}
