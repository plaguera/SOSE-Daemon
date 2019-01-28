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
#define N_FLAGS 16

typedef int bool;
#define true 1
#define false 0

void ErrorExit(const char* format, ...);
int GetPIDbyName(const char* name);
char* GetTimestamp();
void IsAlreadyRunning(const char* process);
int IsDirectory(const char *path);
void KillProcess(const char* process);
bool StringContains(char* string, const char* sub);

#endif
