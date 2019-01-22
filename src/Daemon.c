#include <sys/select.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <ftw.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)


typedef struct {
    int wd;                     /* Watch descriptor (-1 if slot unused) */
    char path[PATH_MAX];        /* Cached pathname */
} node;

int size_tree = 0;
int size_roots = 0;

FILE *logfp = NULL;
node *Tree = NULL;
node *Roots = NULL;

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
	return zero;
}

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
int main(int argc, char *argv[]) {
	int inotifyFd, wd, j;
	char buf[BUF_LEN];
	ssize_t numRead;
	char *p;
	struct inotify_event *event;

	if (argc < 2 || strcmp(argv[1], "--help") == 0) usageError(argv[0]);

	inotifyFd = inotify_init(); /* Create inotify instance */
	if (inotifyFd == -1) errExit("inotify_init");

	size_roots = argc-1;
	node roots[size_roots];
	
	for (j = 1; j < argc; j++) {
	 	wd = inotify_add_watch(inotifyFd, argv[j], IN_ALL_EVENTS);
		
		if (wd == -1) errExit("inotify_add_watch");
		roots[j-1] = Addnode(wd, argv[j]);
		printf("Watching %s using wd %d\n", argv[j], wd);
	}

	for (;;) { // Read events forever
		numRead = read(inotifyFd, buf, BUF_LEN);
		if (numRead == 0) errExit("read() from inotify fd returned 0!");
		if (numRead == -1) errExit("read");
		printf("Read %ld bytes from inotify fd\n", (long) numRead);

		// Process all of the events in buffer returned by read()
		for (p = buf; p < buf + numRead; ) {
			event = (struct inotify_event *) p;
			const char* path = strcat(strcat(NodeFromWD(event->wd).path,"/"), event->name);
			if ((event->mask & IN_CREATE || event->mask & IN_CREATE & IN_ISDIR) && Is_Directory(path)) {
				wd = inotify_add_watch(inotifyFd, path, IN_ALL_EVENTS);
				if (wd == -1) errExit("inotify_add_watch");
				printf("Watching %s using wd %d\n", path, wd);
				Addnode(wd, path);
			}
				
			logInotifyEvent(event);
			p += sizeof(struct inotify_event) + event->len;
		}
	}
	exit(EXIT_SUCCESS);
}

