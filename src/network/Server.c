#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define LOG_FOLDER "../../log/"

struct client_info {
	int sockno;
	char ip[INET_ADDRSTRLEN];
};
FILE *LogFD = NULL;
int clients[100];
int n = 0;
char time_buffer[26];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int keep_going = 1;

char* GetTimestamp() {
	time_t timer;
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);

    strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
	return time_buffer;
}

void LogMessage(const char* ip, const char* format, ...) {
	char buffer[4096];
    va_list args;
    va_start(args, format);
    int rc = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
	fprintf(LogFD, "%s -(%s)- << %s >>\n", GetTimestamp(), ip, buffer);
}

void HandleSignal(int signal) {
	if (signal == SIGTERM) keep_going = 0;
}

void sendtoall(char *msg,int curr)
{
	int i;
	pthread_mutex_lock(&mutex);
	for(i = 0; i < n; i++) {
		if(clients[i] != curr) {
			if(send(clients[i],msg,strlen(msg),0) < 0) {
				perror("sending failure");
				continue;
			}
		}
	}
	pthread_mutex_unlock(&mutex);
}
void *recvmg(void *sock)
{
	struct client_info cl = *((struct client_info *)sock);
	char msg[500];
	int len;
	int i;
	int j;
	while((len = recv(cl.sockno,msg,500,0)) > 0) {
		msg[len] = '\0';
		//sendtoall(msg,cl.sockno);
		LogMessage(cl.ip, "%s",msg);
		memset(msg,'\0',sizeof(msg));
	}
	pthread_mutex_lock(&mutex);
	LogMessage(cl.ip, "Disconnected");
	for(i = 0; i < n; i++) {
		if(clients[i] == cl.sockno) {
			j = i;
			while(j < n-1) {
				clients[j] = clients[j+1];
				j++;
			}
		}
	}
	n--;
	pthread_mutex_unlock(&mutex);
}
int main(int argc,char *argv[])
{
	struct sockaddr_in my_addr,their_addr;
	int my_sock;
	int their_sock;
	socklen_t their_addr_size;
	int portno;
	pthread_t sendt,recvt;
	char msg[500];
	int len;
	struct client_info cl;
	char ip[INET_ADDRSTRLEN];;
	;
	if(argc > 2) {
		printf("too many arguments");
		exit(1);
	}

	/* Our process ID and Session ID */
	pid_t pid, sid;
	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) exit(EXIT_FAILURE);
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		printf("'%s' running in the background. PID = %d\n", argv[0], pid);
		exit(EXIT_SUCCESS);
	}

	struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = HandleSignal;
    sigaction(SIGTERM, &action, NULL);

	char log[1024];
	sprintf(log, "%sserver-log-%s.log", LOG_FOLDER, GetTimestamp());
	LogFD = fopen(log, "w");

	portno = atoi(argv[1]);
	my_sock = socket(AF_INET,SOCK_STREAM,0);
	memset(my_addr.sin_zero,'\0',sizeof(my_addr.sin_zero));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(portno);
	my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	their_addr_size = sizeof(their_addr);

	if(bind(my_sock,(struct sockaddr *)&my_addr,sizeof(my_addr)) != 0) {
		perror("binding unsuccessful");
		exit(1);
	}

	if(listen(my_sock,5) != 0) {
		perror("listening unsuccessful");
		exit(1);
	}

	while(keep_going) {
		if((their_sock = accept(my_sock,(struct sockaddr *)&their_addr,&their_addr_size)) < 0) {
			perror("accept unsuccessful");
			exit(1);
		}
		pthread_mutex_lock(&mutex);
		inet_ntop(AF_INET, (struct sockaddr *)&their_addr, ip, INET_ADDRSTRLEN);
		LogMessage(ip, "Connected");
		cl.sockno = their_sock;
		strcpy(cl.ip,ip);
		clients[n] = their_sock;
		n++;
		pthread_create(&recvt,NULL,recvmg,&cl);
		pthread_mutex_unlock(&mutex);
	}
	fclose(LogFD);
	return 0;
}
