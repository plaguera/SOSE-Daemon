#include "Server.h"

const char* Flags[] = {"IN_ACCESS", "IN_ATTRIB", "IN_CLOSE_NOWRITE", "IN_CLOSE_WRITE", "IN_CREATE", "IN_DELETE", "IN_DELETE_SELF", "IN_IGNORED","IN_ISDIR", "IN_MODIFY", "IN_MOVE_SELF", "IN_MOVED_FROM", "IN_MOVED_TO", "IN_OPEN", "IN_Q_OVERFLOW", "IN_UNMOUNT"};
int FlagCounts[N_FLAGS];

FILE *LogFD = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t keep_going = 1;

client_data* clients;
int n_clients = 0;
char log_path[1024];

int main(int argc,char *argv[])
{
	// Stop Daemon if it is already running
	if (argc == 2 && strcmp(argv[1], "stop") == 0) KillProcess(argv[0]);
	if(argc < 2) ErrorExit("Server port_number");
	if(argc > 2) ErrorExit("too many arguments");

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
	
	// Handle SIGTERM for graceful stop
	struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = HandleSignal;
    sigaction(SIGTERM, &action, NULL);
	
	// Generate log file name based on current timestamp
	snprintf(log_path, 1024, "%sserver-log-%s.log", LOG_FOLDER, GetTimestamp());
	// Launch logging thread
	pthread_t logt, recvt;
	pthread_create(&logt, NULL, Log, NULL);

	struct sockaddr_in my_addr,their_addr;
	int my_sock, their_sock, port;
	socklen_t their_addr_size;
	struct client_info cl;
	char ip[INET_ADDRSTRLEN];	
	
	// Fill server connection struct
	port = atoi(argv[1]);
	my_sock = socket(AF_INET,SOCK_STREAM,0);
	memset(my_addr.sin_zero,'\0',sizeof(my_addr.sin_zero));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	their_addr_size = sizeof(their_addr);

	// Bind to address and port
	if(bind(my_sock, (struct sockaddr *)&my_addr, sizeof(my_addr)) != 0)
		ErrorExit("Binding Unsuccessful");
	
	// Listen on socket, max queue size = 5
	if(listen(my_sock, 5) != 0)
		ErrorExit("Listening Unsuccessful");
	
	// Listen for new clients
	while(keep_going) {
		// Accept new connections
		if((their_sock = accept(my_sock, (struct sockaddr*)&their_addr, &their_addr_size)) < 0)
			if(keep_going) ErrorExit("Accept Unsuccessful");
		
		pthread_mutex_lock(&mutex);
		inet_ntop(AF_INET, (struct sockaddr *)&their_addr, ip, INET_ADDRSTRLEN);
		AddClient(ip);
		cl.sockno = their_sock;
		strcpy(cl.ip,ip);
		pthread_create(&recvt, NULL, ReceiveMessages, &cl);
		pthread_mutex_unlock(&mutex);
	}
	shutdown(my_sock, SHUT_RDWR);
	return 0;
}

void HandleSignal(int signal) {
	if (signal == SIGTERM) keep_going = 0;
}
	
void* ReceiveMessages(void *sock) {
	struct client_info cl = *((struct client_info *)sock);
	int len = 0;
	char buf[MSG_LENGTH];
	while((len = read(cl.sockno, buf, MSG_LENGTH)) > 0) {
		//printf("%s -- %s\n", cl.ip, buf);
		AddMessage(cl.ip, buf);
		bzero(buf,MSG_LENGTH);
		//write(cl.sockno, "Message Received", sizeof("Message Received"));
	}
}

void* Log(void *arg) {
	while (keep_going) {
		sleep(10);
		LogFD = fopen(log_path, "w+");
		if (LogFD == NULL) ErrorExit("fopen(%s)", log_path);
		for (int i = 0; i < n_clients; i++)
			LogClient(clients[i]);
		fclose(LogFD);
	}
}

void LogClient(client_data client) {
	fprintf(LogFD, "< %s >:\n", client.ip);
	LogMessages(client.n_messages, client.messages);	
}

void LogMessages(int n, message* messages) {
	for (int i = 0; i < n; i++) {
		if (StringContains(messages[i].text, "Connected"))
			fprintf(LogFD, "\tConnected on %s\n", messages[i].time);
		for (int j = 0; j < N_FLAGS; j++)
			if (StringContains(messages[i].text, Flags[j]))
				FlagCounts[j]++;
	}
	for (int i = 0; i < N_FLAGS; i++)
		fprintf(LogFD, "\t%s: %d times\n", Flags[i], FlagCounts[i]);
	fprintf(LogFD, "\n");
	for (int i = 0; i < N_FLAGS; i++) Flags[i] = 0;
}

void AddClient(const char* ip) {
	n_clients++;
	clients = realloc(clients, n_clients * sizeof(client_data));
	strcpy(clients[n_clients-1].ip, ip);
	clients[n_clients-1].n_messages = 0;
}

void AddMessage(const char* ip, char* message) {
	for (int i = 0; i < n_clients; i++)
		if (strcmp(clients[i].ip, ip) == 0) {
			clients[i].n_messages++;
			strcpy(clients[i].messages[clients[i].n_messages-1].time, GetTimestamp());
			strcpy(clients[i].messages[clients[i].n_messages-1].text, message);
			return;
		}
}
