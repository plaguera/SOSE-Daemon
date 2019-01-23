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
#define LOG_FILE "daemon.log"


typedef struct {
    int wd;
    char path[PATH_MAX];
} node;

int inotify_fd = 0;
int size_tree = 0;

FILE *LogFD = NULL;
node *Tree = NULL;
char time_buffer[26];

node Zero;

int FindEmptyAddress();

int Is_Directory(const char *path);

node AddNode(int wd, const char *path);

void DeleteNode(int wd, const char *path);

void WatchNode(const char* path);

void LogEvent(struct inotify_event *i);

void UsageError(const char *proccess_name);

node NodeFromWD(int wd);

void TraverseDirectory(const char *name);

int main(int argc, char *argv[]) {
	if (argc < 2 || strcmp(argv[1], "--help") == 0) UsageError(argv[0]);

	int wd, i;
	char buffer[BUF_LEN];
	ssize_t bytes_read;
	struct inotify_event *event;

	inotify_fd = inotify_init();
	if (inotify_fd == -1) ErrorExit("inotify_init");
	
	for (i = 1; i < argc; i++) {
		WatchNode(argv[i]);
	 	TraverseDirectory(argv[i]);
	}

	char *p;
	while (1) {
		bytes_read = read(inotify_fd, buffer, BUF_LEN);
		if (bytes_read <= 0) ErrorExit("read()");
		printf("Read %ld bytes from inotify fd\n", (long) bytes_read);

		for (p = buffer; p < buffer + bytes_read; ) {
			event = (struct inotify_event *) p;
			const char* wd_path = NodeFromWD(event->wd).path;

			if (event->mask & IN_CREATE || event->mask & IN_CREATE & IN_ISDIR) {
				char path[PATH_MAX + NAME_MAX];
				snprintf(path, sizeof(path), "%s/%s", wd_path, event->name);
				if (Is_Directory(path)) WatchNode(path);
			}
			else if (event->mask & IN_DELETE_SELF) {
				inotify_rm_watch(inotify_fd, event->wd);
				printf("Stopped Watching %s using wd %d\n", wd_path, event->wd);
				DeleteNode(event->wd, wd_path);
			}
			else if (event->mask & IN_MOVED_TO & IN_ISDIR) {
				printf("HOLA\n");
				char path[PATH_MAX + NAME_MAX];
				snprintf(path, sizeof(path), "%s/%s", wd_path, event->name);
				if (Is_Directory(path)) WatchNode(path);
			}
			LogEvent(event);
			p += sizeof(struct inotify_event) + event->len;
		}
	}
	exit(EXIT_SUCCESS);
}

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

int Is_Directory(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

node AddNode(int wd, const char *path)
{
    int i = FindEmptyAddress();
	Tree[i].wd = wd;
	strncpy(Tree[i].path, path, PATH_MAX);
	return Tree[i];
}

void DeleteNode(int wd, const char *path)
{
	int i;
    for (i = 0; i < size_tree; i++)
        if (Tree[i].wd == wd) {
			Tree[i].wd = -1;
			strcpy(Tree[i].path, "");
			break;
		}
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

	LogFD = fopen(LOG_FILE, "a+");

	fprintf(LogFD, "%s -- wd =%2d; ", GetTimestamp(), i->wd);
	if (i->cookie > 0) fprintf(LogFD, "cookie =%4d; ", i->cookie);
	fprintf(LogFD, "mask = ");
	if (i->mask & IN_ACCESS) 		fprintf(LogFD, "IN_ACCESS ");
	if (i->mask & IN_ATTRIB) 		fprintf(LogFD, "IN_ATTRIB ");
	if (i->mask & IN_CLOSE_NOWRITE) fprintf(LogFD, "IN_CLOSE_NOWRITE ");
	if (i->mask & IN_CLOSE_WRITE)	fprintf(LogFD, "IN_CLOSE_WRITE ");
	if (i->mask & IN_CREATE) 		fprintf(LogFD, "IN_CREATE ");
	if (i->mask & IN_DELETE) 		fprintf(LogFD, "IN_DELETE ");
	if (i->mask & IN_DELETE_SELF) 	fprintf(LogFD, "IN_DELETE_SELF ");
	if (i->mask & IN_IGNORED) 		fprintf(LogFD, "IN_IGNORED ");
	if (i->mask & IN_ISDIR) 		fprintf(LogFD, "IN_ISDIR ");
	if (i->mask & IN_MODIFY) 		fprintf(LogFD, "IN_MODIFY ");
	if (i->mask & IN_MOVE_SELF) 	fprintf(LogFD, "IN_MOVE_SELF ");
	if (i->mask & IN_MOVED_FROM) 	fprintf(LogFD, "IN_MOVED_FROM ");
	if (i->mask & IN_MOVED_TO) 		fprintf(LogFD, "IN_MOVED_TO ");
	if (i->mask & IN_OPEN) 			fprintf(LogFD, "IN_OPEN ");
	if (i->mask & IN_Q_OVERFLOW) 	fprintf(LogFD, "IN_Q_OVERFLOW ");
	if (i->mask & IN_UNMOUNT) 		fprintf(LogFD, "IN_UNMOUNT ");
	fprintf(LogFD, "; ");
	if (i->len > 0) fprintf(LogFD, "name = %s;", i->name);
	fprintf(LogFD, "\n");
	fclose(LogFD);
}

void UsageError(const char *proccess_name)
{
    fprintf(stderr, "Usage: %s directory-path\n\n", proccess_name);
    exit(EXIT_FAILURE);
}

node NodeFromWD(int wd)
{
	int i;
	for(i = 0; i < size_tree; i++)	
		if (Tree[i].wd == wd)
			return Tree[i];
	node zero;
	return Zero;
}

void WatchNode(const char* path)
{
	int wd = inotify_add_watch(inotify_fd, path, IN_ALL_EVENTS);
	if (wd == -1) ErrorExit("inotify_add_watch");
	printf("Watching %s using wd %d\n", path, wd);
	AddNode(wd, path);
}

void TraverseDirectory(const char *name)
{
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            char path[PATH_MAX + NAME_MAX];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            WatchNode(path);
            TraverseDirectory(path);
        }
    }
    closedir(dir);
}

