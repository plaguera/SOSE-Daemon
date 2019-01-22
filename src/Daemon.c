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
#include <unistd.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)


typedef struct {
    int wd;                     /* Watch descriptor (-1 if slot unused) */
    char path[PATH_MAX];        /* Cached pathname */
} node;

int inotify_fd = 0;
int size_tree = 0;

FILE *logfp = NULL;
node *Tree = NULL;

node Zero;

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

node Addnode(int wd, const char *path)
{
    int i = FindEmptyAddress();
	Tree[i].wd = wd;
	strncpy(Tree[i].path, path, PATH_MAX);
	return Tree[i];
}

void Deletenode(int wd, const char *path)
{
	int i;
    for (i = 0; i < size_tree; i++)
        if (Tree[i].wd == wd) {
			Tree[i].wd = -1;
			strcpy(Tree[i].path, "");
			break;
		}
}

/* Display information from inotify_event structure */
 void logInotifyEvent(struct inotify_event *i) {
	printf(" wd =%2d; ", i->wd);
	if (i->cookie > 0) printf("cookie =%4d; ", i->cookie);
	printf("mask = ");
	if (i->mask & IN_ACCESS) printf("IN_ACCESS ");
	if (i->mask & IN_ATTRIB) printf("IN_ATTRIB ");
	if (i->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
	if (i->mask & IN_CLOSE_WRITE) printf("IN_CLOSE_WRITE ");
	if (i->mask & IN_CREATE) printf("IN_CREATE ");
	if (i->mask & IN_DELETE) printf("IN_DELETE ");
	if (i->mask & IN_DELETE_SELF) printf("IN_DELETE_SELF ");
	if (i->mask & IN_IGNORED) printf("IN_IGNORED ");
	if (i->mask & IN_ISDIR) printf("IN_ISDIR ");
	if (i->mask & IN_MODIFY) printf("IN_MODIFY ");
	if (i->mask & IN_MOVE_SELF) printf("IN_MOVE_SELF ");
	if (i->mask & IN_MOVED_FROM) printf("IN_MOVED_FROM ");
	if (i->mask & IN_MOVED_TO) printf("IN_MOVED_TO ");
	if (i->mask & IN_OPEN) printf("IN_OPEN ");
	if (i->mask & IN_Q_OVERFLOW) printf("IN_Q_OVERFLOW ");
	if (i->mask & IN_UNMOUNT) printf("IN_UNMOUNT ");
	printf("\n");
	if (i->len > 0) printf(" name = %s\n", i->name);
}

 void usageError(const char *pname)
{
    fprintf(stderr, "Usage: %s directory-path\n\n", pname);
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
	if (wd == -1) errExit("inotify_add_watch");
	printf("Watching %s using wd %d\n", path, wd);
	Addnode(wd, path);
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

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
int main(int argc, char *argv[]) {
	int wd, j;
	char buf[BUF_LEN];
	ssize_t numRead;
	char *p;
	struct inotify_event *event;

	if (argc < 2 || strcmp(argv[1], "--help") == 0) usageError(argv[0]);

	inotify_fd = inotify_init(); /* Create inotify instance */
	if (inotify_fd == -1) errExit("inotify_init");
	
	for (j = 1; j < argc; j++) {
		WatchNode(argv[j]);
	 	TraverseDirectory(argv[j]);
	}

	for (;;) { // Read events forever
		numRead = read(inotify_fd, buf, BUF_LEN);
		if (numRead <= 0) errExit("read()");
		printf("Read %ld bytes from inotify fd\n", (long) numRead);

		// Process all of the events in buffer returned by read()
		for (p = buf; p < buf + numRead; ) {
			event = (struct inotify_event *) p;
			const char* wd_path = NodeFromWD(event->wd).path;

			if ((event->mask & IN_CREATE || event->mask & IN_CREATE & IN_ISDIR)) {
				char path[PATH_MAX + NAME_MAX];
				snprintf(path, sizeof(path), "%s/%s", wd_path, event->name);
				if (Is_Directory(path)) WatchNode(path);
			}
			else if (event->mask & IN_DELETE_SELF) {
				inotify_rm_watch(inotify_fd, event->wd);
				printf("Stopped Watching %s using wd %d\n", wd_path, event->wd);
				Deletenode(event->wd, wd_path);
			}
			//logInotifyEvent(event);
			p += sizeof(struct inotify_event) + event->len;
		}
	}
	exit(EXIT_SUCCESS);
}

