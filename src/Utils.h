#ifndef UTILS_H_
#define UTILS_H_

#include <dirent.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MSG_LENGTH 1024

void ErrorExit(const char* format, ...);
int GetPIDbyName(const char* name);
char* GetTimestamp();
void IsAlreadyRunning(const char* process);
int IsDirectory(const char *path);
void KillProcess(const char* process);

#endif
