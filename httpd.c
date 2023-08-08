#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEFAULT_BACKLOG 100
#define MESG "Hello, TCP Client.\n"
#define MAXLEN 1000
//global vars
int mlen, sock_fd, newsock, fifo_fd, cf_fd;
char* fname;
int i = 0;
char client_fifo[20];

int sendError(char *msg, int sock)
{
	mlen = send(sock, msg, strlen(msg), 0);
	char *length = "Content-Length: 0\r\n\r\n";
	send(sock,length, strlen(length), 0);
	return mlen;
}

void writeHeader(char* fname, int sock) {
	FILE *fp;
	struct stat fstat;
	fp = fopen(fname, "rb");
    if (fp == NULL) {
		printf("error: unable to open file.\n");
		sendError("HTTP/1.1 404 Not Found\r\n", sock);
        free(fname);
		exit(-1);
    }
	char *valid = "HTTP/1.1 200 OK";
    char *type = "Content-Type: text/html";
    stat(fname, &fstat);
    int size = fstat.st_size;
    char length[100];
    sprintf(length, "Content-Length: %d", size);
	char *msg = (char*) malloc(strlen(valid)+strlen(type)+strlen(length)+11);
	memset(msg,0,sizeof(msg));
	sprintf(msg,"%s\r\n%s\r\n%s\r\n\r\n", valid, type, length);
	write(STDOUT_FILENO,msg,strlen(msg));
	mlen = send(sock, msg, strlen(msg), 0);
	free(msg);
    fclose(fp);
}

void kvPut(char* key, char* buff, char* server_fifo, int sock) {
	//special server response
	char *valid = "HTTP/1.1 200 OK";
    char *length = "Content-Length: 0";
   	char *msg = (char*) malloc(strlen(valid)+strlen(length)+7);
    memset(msg,0,strlen(valid)+strlen(length)+7);
    sprintf(msg, "%s\r\n%s\r\n\r\n", valid, length);
	mlen = send(sock, msg, strlen(msg), 0);
	free(msg);

	i = strlen(buff);
    while(buff[i] != '\r') {
    	i--;
    } //reads last line
   	i+=2;
    int val_size = (strlen(buff) - i) + 1;
    char val[val_size];
    memset(val,'\0',val_size);
    memcpy(val, &buff[i], val_size - 1);
	
	char request[40 + val_size];
	memset(request, '\0', sizeof(request));
	sprintf(request, "set %s %s", key, val);
	fifo_fd = open(server_fifo, O_WRONLY);
	write (fifo_fd, request, strlen(request) + 1);
	memset(key, 0, sizeof(key));
	close(fifo_fd);	

}

void kvGet(char* key, char* server_fifo, int clientID, int sock) {
	char request[50];
	char* error;
	memset(request, '\0', sizeof(request));
	sprintf(request, "get %d %s", clientID, key);
	fifo_fd = open(server_fifo, O_WRONLY);
	write (fifo_fd, request, strlen(request) +1);

	char val[1025];
	memset(val, 0, sizeof(val));
	sprintf(client_fifo, "client:%d", clientID);
	cf_fd = open(client_fifo, O_RDONLY);
	int x = 0;
	x = read(cf_fd, &val, 1024);
	error = (char*) malloc (20+strlen(key)+1);//key [key] does not exist, %key
	memset(error, 0, 20+strlen(key) + 1);
	sprintf(error,"Key %s does not exist.",key);
	if (strcmp(error, val) == 0) {
		sendError("HTTP/1.1 404 Not Found\r\n", sock);
    	free(error);
		free(fname);
	    exit(-1);
	}
	free(error);
	//special response
	char *valid = "HTTP/1.1 200 OK";
	char length[100];
    sprintf(length, "Content-Length: %ld", strlen(val));

	char *msg = (char*) malloc(strlen(valid) + strlen(length) + strlen(val) + 11);
	memset(msg, 0, strlen(valid) + strlen(val) + 11);
	sprintf(msg, "%s\r\n%s\r\n\r\n%s", valid, length, val);
	mlen = send(sock, msg, strlen(msg), 0);	
	free(msg);
	close(cf_fd);
	close(fifo_fd);
	unlink(client_fifo);
}

void handle_sigquit(int sig)
{
	close(cf_fd);
    close(fifo_fd);
    unlink(client_fifo);
    shutdown(sock_fd, SHUT_RDWR);
    shutdown(newsock, SHUT_RDWR);
    exit(0);
}


int main(int argc, char *argv[]) {
    struct sockaddr_in sa, newsockinfo, peerinfo;
    socklen_t len;
	FILE *fp;
	struct stat fstat;
    
	char localaddr[INET_ADDRSTRLEN], peeraddr[INET_ADDRSTRLEN], buff[MAXLEN+1];
    memset(sa.sin_zero, 0, sizeof(sa.sin_zero));
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if (argc < 3) {
        printf("usage: ./httpd <fifo name> <port num>");
        exit(-1);
    }
    
	int port = atoi(argv[2]);
	sa.sin_port = htons(port);

	if (bind(sock_fd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
		perror("bind error");
		exit(-1);
	}
    
	if (listen(sock_fd, DEFAULT_BACKLOG) < 0) {
		perror("listen error");
        exit(-1);
	}
    
	while (1) {
		signal(SIGQUIT, handle_sigquit);
		int pid;
    	len = sizeof(newsockinfo); 
    	newsock = accept(sock_fd, (struct sockaddr *) &peerinfo, &len);
		if (newsock < 0) {
			perror("connection error");
			exit(-1);
		}

		if ((pid = fork()) == -1) {
			sendError("HTTP/1.1 500 Internal Error\r\n", newsock);
			shutdown(newsock, SHUT_RDWR);
			exit(-1);
		}

		else if (pid == 0) {
			mlen = 0;
			memset(buff,0,sizeof(buff));
			mlen = recv(newsock, buff, sizeof(buff), 0);
			char request[5];
			fname = (char*) malloc(sizeof(buff));
			memset (fname, 0, sizeof(buff));
			memset(request,0,sizeof(request));
			int i = 0;
			while (buff[i] != ' ') { //init request
				request[i] = buff[i];
				i++;
			}
			request[i] = '\0';

			i += 2;
			int j = 0;
			while (buff[i] != ' ') { //init fname
                fname[j] = buff[i];
				i++; j++;
            }
			fname[j] = '\0';
	
			//kv functionality	
			if (fname[0] == 'k' && fname[1] == 'v' && fname[2] == '/')  {
				char key[33];
				memset(key, 0, sizeof(key));
				memcpy(key, &fname[3], strlen(fname)-3); //init key
				//kv get request
				if (strcmp("GET", request) == 0) {
					int clientID;
					clientID = getpid();
					kvGet(key, argv[1], clientID, newsock);
				}	
				//kv put request
				if (strcmp("PUT", request) == 0) {
					kvPut(key, buff, argv[1], newsock);
				}
				free(fname);
			}

		//html requests
			else if (strcmp("HEAD", request) == 0) {
				writeHeader(fname, newsock);
				free(fname);	
			}
			
			else if (fname != NULL && strcmp("GET", request) == 0 && fname[0] != 'k' && fname[1] != 'v') {
				writeHeader(fname, newsock);
				fp = fopen(fname, "rb");
				stat(fname, &fstat);
    			int size = fstat.st_size;
				char* contents = (char*) malloc (size);
				fread(contents, 1, size, fp);
				write(STDOUT_FILENO, contents, size); //strlen(contents)
				mlen = send(newsock, contents, size, 0);
				free(contents);
				fclose(fp);	
				free(fname);
			}
			else if (strcmp("PUT", request) == 0 && fname[0] != 'k' && fname[1] != 'v') {
				sendError("HTTP/1.1 403 Permission Denied\r\n", newsock);	
				free(fname);
				exit(-1);
			}
			
			else if (strcmp("GET", request) != 0 && strcmp("PUT", request) != 0 && strcmp("HEAD", request) != 0) {
				sendError("HTTP/1.1 501 Not Implemented\r\n", newsock);
            	free(fname);
			    exit(-1);	
			}

			else {
                sendError("HTTP/1.1 400 Bad Request\r\n", newsock);
                free(fname);
				exit(-1);
            }
		}
		//waiting for child
		int status;
		pid_t child;
		child = wait(&status);
//		shutdown(sock_fd, SHUT_RDWR);
  //  	shutdown(newsock, SHUT_RDWR);		
	}	
    
    //len = sizeof(newsockinfo);
    //getsockname(newsock, (struct sockaddr *) &newsockinfo, &len);
    
    inet_ntop(AF_INET, &newsockinfo.sin_addr.s_addr, localaddr, sizeof(localaddr));
    inet_ntop(AF_INET, &peerinfo.sin_addr.s_addr, peeraddr, sizeof(peeraddr));
    
    printf("New Connection: %s:%d->%s:%d\n", peeraddr, ntohs(peerinfo.sin_port), localaddr, ntohs(newsockinfo.sin_port));
    

   // mlen = recv(newsock, buff, sizeof(buff), 0);
   // write(STDOUT_FILENO,buff,mlen);
   // mlen = send(newsock, MESG, strlen(MESG), 0);
    
    shutdown(sock_fd, SHUT_RDWR);
    shutdown(newsock, SHUT_RDWR);
    
    return 0;
}
