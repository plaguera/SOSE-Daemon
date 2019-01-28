#include "Server.h"

FILE *LogFD = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t keep_going = 1;

int main(int argc,char *argv[])
{
	// Stop Daemon if it is already running
	if (argc == 2 && strcmp(argv[1], "stop") == 0) KillProcess(argv[0]);
	struct sockaddr_in my_addr,their_addr;
	int my_sock, their_sock, portno;
	socklen_t their_addr_size;
	pthread_t sendt, recvt;
	struct client_info cl;
	char ip[INET_ADDRSTRLEN];
	if(argc > 2) {
		printf("too many arguments");
		exit(1);
	}

	/* Our process ID and Session ID */
	pid_t pid, sid;
	/* Fork off the parent process */
	//pid = fork();
	//if (pid < 0) exit(EXIT_FAILURE);
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		//printf("'%s' running in the background. PID = %d\n", argv[0], pid);
		//exit(EXIT_SUCCESS);
	}

	struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = HandleSignal;
    sigaction(SIGTERM, &action, NULL);

	char log[1024];
	snprintf(log, 1024, "%sserver-log-%s.log", LOG_FOLDER, GetTimestamp());
	LogFD = fopen(log, "w+");
	if (LogFD == NULL) ErrorExit("fopen(%s)", log);

	portno = atoi(argv[1]);
	my_sock = socket(AF_INET,SOCK_STREAM,0);
	memset(my_addr.sin_zero,'\0',sizeof(my_addr.sin_zero));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(portno);
	my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	their_addr_size = sizeof(their_addr);

	if(bind(my_sock,(struct sockaddr *)&my_addr,sizeof(my_addr)) != 0)
		ErrorExit("Binding Unsuccessful");

	if(listen(my_sock,5) != 0)
		ErrorExit("Listening Unsuccessful");

	while(keep_going) {
		if((their_sock = accept(my_sock, (struct sockaddr*)&their_addr, &their_addr_size)) < 0)
			if(keep_going) ErrorExit("Accept Unsuccessful");

		pthread_mutex_lock(&mutex);
		inet_ntop(AF_INET, (struct sockaddr *)&their_addr, ip, INET_ADDRSTRLEN);

		//LogMessage(ip, "Connected");
		cl.sockno = their_sock;
		strcpy(cl.ip,ip);
		pthread_create(&recvt,NULL,ReceiveMessages,&cl);
		pthread_mutex_unlock(&mutex);
	}
	fclose(LogFD);
	shutdown(my_sock, SHUT_RDWR);
	return 0;
}

void LogMessagef(const char* ip, const char* format, ...) {
	char buffer[MSG_LENGTH];
    va_list args;
    va_start(args, format);
    int rc = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
	//fprintf(LogFD, "%s -(%s)- << %s >>\n", GetTimestamp(), ip, buffer);
	//printf("%s -(%s)- << %s >>\n", GetTimestamp(), ip, buffer);
}

void LogMessage(const char* ip, const char* message) {
	//fprintf(LogFD, "%s -(%s)- << %s >>\n", GetTimestamp(), ip, message);
	printf("%s -(%s)- << %s >>\n", GetTimestamp(), ip, message);
}

void HandleSignal(int signal) {
	if (signal == SIGTERM) keep_going = 0;
}

	char buf[MSG_LENGTH];
void* ReceiveMessages(void *sock)
{
	struct client_info cl = *((struct client_info *)sock);
	int len = 0;
 bzero(buf,MSG_LENGTH);
	while((len = read(cl.sockno, buf, sizeof(buf))) > 0) {
		printf("'%s'\n", buf);
		//LogMessage(cl.ip, buf);
		bzero(buf,MSG_LENGTH);
	}
}
