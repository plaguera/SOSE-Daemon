#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
#define ErrorExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define LOG_FOLDER "../log/"

typedef int bool;
#define true 1
#define false 0

bool stop = false;

typedef struct {
    uint32_t wd;
    char path[PATH_MAX];
} node;
node Zero;

int inotify_fd = 0;
int size_tree = 0;

FILE *LogFD = NULL;
node *Tree = NULL;
char time_buffer[26];
volatile sig_atomic_t keep_going = 1;

node AddNode(uint32_t wd, const char *path);
void AddWatch(const char* path);
void DeleteNode(uint32_t wd, const char *path);
int FindEmptyAddress();
int GetPIDbyName(char* name);
char* GetTimestamp();
void HandleInotifyEvent(struct inotify_event * event);
void HandleSignal(int signal);
int Is_Directory(const char *path);
void LogEvent(struct inotify_event *i);
void LogMessage(const char* format, ...);
node NodeFromWD(uint32_t wd);
void RecursiveAddWatch(const char *name);
void RecursiveStopWatch(const char *name);
void StopWatch(uint32_t  wd);
void UsageError(const char *proccess_name);

int main(int argc, char *argv[]) {

	if (argc == 2 && strcmp(argv[1], "stop") == 0) {
		int pid_from_name = GetPIDbyName(argv[0]);
		if (pid_from_name >= 0) {
			kill(pid_from_name, SIGTERM);
			printf("Killed '%s'!. PID = %d\n", argv[0], pid_from_name);
			exit(EXIT_SUCCESS);
		}
		ErrorExit("'Daemon' is not running!\n");
	}
	if (argc < 2 || strcmp(argv[1], "--help") == 0) UsageError(argv[0]);

	int pid_from_name = GetPIDbyName(argv[0]);
	if (pid_from_name >= 0) {
		printf("'%s' is already running in the background!. PID = %d\n", argv[0], pid_from_name);
		exit(EXIT_FAILURE);
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

	int i;
	for (i = 1; i < argc; i++) {
		if (!Is_Directory(argv[i])) ErrorExit("Only Directories can be Watched!\n");
		if (argv[i][strlen(argv[i]) - 1] == '/') argv[i][strlen(argv[i]) - 1] = '\0';
	}

	int wd;
	char buffer[BUF_LEN];
	ssize_t bytes_read;
	struct inotify_event *event;

	inotify_fd = inotify_init();
	if (inotify_fd == -1) ErrorExit("inotify_init");
	
	char log[1024];
	sprintf(log, "%sdaemon-log-%s.log", LOG_FOLDER, GetTimestamp());
	LogFD = fopen(log, "w");
	for (i = 1; i < argc; i++) RecursiveAddWatch(argv[i]);

	char *p;
	while (keep_going) {
		bytes_read = read(inotify_fd, buffer, BUF_LEN);
		if (keep_going && bytes_read <= 0) ErrorExit("read()");
		LogMessage("Read %ld bytes from inotify fd", (long) bytes_read);

		for (p = buffer; p < buffer + bytes_read; ) {
			event = (struct inotify_event *) p;
			HandleInotifyEvent(event);
			p += sizeof(struct inotify_event) + event->len;
		}
	}
	fclose(LogFD);
	close(inotify_fd);
	exit(EXIT_SUCCESS);
}

void HandleInotifyEvent(struct inotify_event * event) {
	const char* wd_path = NodeFromWD(event->wd).path;
	if 	(event->mask & IN_CREATE || 
		(event->mask & IN_CREATE && event->mask & IN_ISDIR)) {
		char path[PATH_MAX + NAME_MAX];
		snprintf(path, sizeof(path), "%s/%s", wd_path, event->name);
		if (Is_Directory(path)) AddWatch(path);
	}
	else if (event->mask & IN_DELETE_SELF) StopWatch(event->wd);
	else if (event->mask & IN_MOVED_TO && event->mask & IN_ISDIR) {
		char path[PATH_MAX + NAME_MAX];
		snprintf(path, sizeof(path), "%s/%s", wd_path, event->name);
		if (Is_Directory(path)) AddWatch(path);
	}
	LogEvent(event);
}

void HandleSignal(int signal) {
	if (signal == SIGTERM) keep_going = 0;
}

int Is_Directory(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

int GetPIDbyName(char* name)
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];
    
    if (!(dir = opendir("/proc"))) {
        perror("can't open /proc");
        return -1;
    }

    while((ent = readdir(dir)) != NULL) {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') continue;

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
        FILE* fp = fopen(buf, "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp) != NULL) {
                /* check the first token in the file, the program name */
                char* first = strtok(buf, " ");
                if (!strcmp(first, name) && (pid_t)lpid != getpid()) {
                    fclose(fp);
                    closedir(dir);
                    return (pid_t)lpid;
                }
            }
            fclose(fp);
        }
    }
    
    closedir(dir);
    return -1;
}

char* GetTimestamp() {
	time_t timer;
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);

    strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
	return time_buffer;
}

void LogEvent(struct inotify_event *i) {
	fprintf(LogFD, "%s -- wd =%2d; ", GetTimestamp(), i->wd);
	if (i->cookie > 0) fprintf(LogFD, "cookie =%4d; ", i->cookie);
	fprintf(LogFD, "mask =");
	if (i->mask & IN_ACCESS) 		fprintf(LogFD, " IN_ACCESS");
	if (i->mask & IN_ATTRIB) 		fprintf(LogFD, " IN_ATTRIB");
	if (i->mask & IN_CLOSE_NOWRITE) fprintf(LogFD, " IN_CLOSE_NOWRITE");
	if (i->mask & IN_CLOSE_WRITE)	fprintf(LogFD, " IN_CLOSE_WRITE");
	if (i->mask & IN_CREATE) 		fprintf(LogFD, " IN_CREATE");
	if (i->mask & IN_DELETE) 		fprintf(LogFD, " IN_DELETE");
	if (i->mask & IN_DELETE_SELF) 	fprintf(LogFD, " IN_DELETE_SELF");
	if (i->mask & IN_IGNORED) 		fprintf(LogFD, " IN_IGNORED");
	if (i->mask & IN_ISDIR) 		fprintf(LogFD, " IN_ISDIR");
	if (i->mask & IN_MODIFY) 		fprintf(LogFD, " IN_MODIFY");
	if (i->mask & IN_MOVE_SELF) 	fprintf(LogFD, " IN_MOVE_SELF");
	if (i->mask & IN_MOVED_FROM) 	fprintf(LogFD, " IN_MOVED_FROM");
	if (i->mask & IN_MOVED_TO) 		fprintf(LogFD, " IN_MOVED_TO");
	if (i->mask & IN_OPEN) 			fprintf(LogFD, " IN_OPEN");
	if (i->mask & IN_Q_OVERFLOW) 	fprintf(LogFD, " IN_Q_OVERFLOW");
	if (i->mask & IN_UNMOUNT) 		fprintf(LogFD, " IN_UNMOUNT");
	fprintf(LogFD, "; ");
	if (i->len > 0) fprintf(LogFD, "name = %s;", i->name);
	fprintf(LogFD, "\n");
}

void LogMessage(const char* format, ...) {
	char buffer[4096];
    va_list args;
    va_start(args, format);
    int rc = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
	fprintf(LogFD, "%s -- << \"%s\" >>\n", GetTimestamp(), buffer);
}

void UsageError(const char *proccess_name)
{
    fprintf(stderr, "Usage: %s directory-path\n\n", proccess_name);
    exit(EXIT_FAILURE);
}

#pragma region NodeFunctions

int FindEmptyAddress()
{
	int i;
    for (i = 0; i < size_tree; i++)
        if (Tree[i].wd <= 0)
            return i;
	size_tree++;
	Tree = realloc(Tree, size_tree * sizeof(node));
	return size_tree-1;
}

bool IsBeingWatched(const char* path) {
	int i;
	for(i = 0; i < size_tree; i++)	
		if (strcmp(path, Tree[i].path) == 0)
			return true;
	return false;
}

node NodeFromWD(uint32_t wd)
{
	int i;
	for(i = 0; i < size_tree; i++)	
		if (Tree[i].wd == wd)
			return Tree[i];
	return Zero;
}

node NodeFromPath(const char* path)
{
	int i;
	for(i = 0; i < size_tree; i++)	
		if (strcmp(path, Tree[i].path) == 0)
			return Tree[i];
	return Zero;
}

void AddWatch(const char* path)
{
	if (IsBeingWatched(path)) return;
	int wd = inotify_add_watch(inotify_fd, path, IN_ALL_EVENTS);
	if (wd == -1) ErrorExit("inotify_add_watch");
	LogMessage("Watching %s using wd %d", path, wd);
	AddNode(wd, path);
}

void StopWatch(uint32_t  wd) {
	const char* path = NodeFromWD(wd).path;
	LogMessage("Stopped Watching %s using wd %d", NodeFromWD(wd).path, wd);
	DeleteNode(wd, path);
}

node AddNode(uint32_t wd, const char *path)
{
    int i = FindEmptyAddress();
	Tree[i].wd = wd;
	strncpy(Tree[i].path, path, PATH_MAX);
	return Tree[i];
}

void DeleteNode(uint32_t wd, const char *path)
{
	int i;
    for (i = 0; i < size_tree; i++) if (Tree[i].wd == wd) Tree[i] = Zero;
}

void RecursiveAddWatch(const char *name) {
	DIR *dir;
    struct dirent *entry;
    if (!(dir = opendir(name))) return;

	AddWatch(name);
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            char path[PATH_MAX];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            RecursiveAddWatch(path);
        }
    }
    closedir(dir);
}

#pragma endregion NodeFunctions
