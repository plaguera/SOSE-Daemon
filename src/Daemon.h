#ifndef DAEMON_H_
#define DAEMON_H_

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <netinet/in.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/socket.h>
#include "Utils.h"

#define LOG_FOLDER "../log/"

typedef int bool;
#define true 1
#define false 0

typedef struct {
    uint32_t wd;
    char path[PATH_MAX];
} node;

void LogMessageToServer(const char* msg);

void HandleInotifyEvent(struct inotify_event * event);

void HandleSignal(int signal);

void LogEvent(struct inotify_event *i);

void LogMessage(const char* format, ...);

void UsageError(const char *proccess_name);

int FindEmptyAddress();

bool IsBeingWatched(const char* path);

node NodeFromWD(uint32_t wd);

node NodeFromPath(const char* path);

void AddWatch(const char* path);

void StopWatch(uint32_t  wd);

node AddNode(uint32_t wd, const char *path);

void DeleteNode(uint32_t wd, const char *path);

void RecursiveAddWatch(const char *name);

#endif
