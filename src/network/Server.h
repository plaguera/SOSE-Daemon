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

void HandleSignal(int signal);
void LogMessage(const char* ip, const char* message);
void LogMessagef(const char* ip, const char* format, ...);
void* ReceiveMessages(void *sock);

#endif
