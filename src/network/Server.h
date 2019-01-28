#ifndef SERVER_H_
#define SERVER_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include "../Utils.h"

#define LOG_FOLDER "../log/"

struct client_info {
	int sockno;
	char ip[INET_ADDRSTRLEN];
};

typedef struct {
	char time[26];
	char text[MSG_LENGTH];
} message;

typedef struct {
	char ip[INET_ADDRSTRLEN];
	int n_messages;
	message messages[1024];
} client_data;

void AddClient(const char* ip);
void AddMessage(const char* ip, char* message);
void HandleSignal(int signal);
void* Log(void *arg);
void LogClient(client_data client);
void LogMessages(int n, message* messages);
void* ReceiveMessages(void *sock);

#endif
