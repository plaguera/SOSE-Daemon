#include "Daemon.h"

node Zero;

int inotify_fd = 0;
int size_watch_list = 0;
int client_socket = 0;
volatile sig_atomic_t keep_going = 1;

FILE *LogFD = NULL;
node *WatchList = NULL;

int main(int argc, char *argv[]) {
	// Stop Daemon if it is already running
	if (argc == 2 && strcmp(argv[1], "stop") == 0) KillProcess(argv[0]);
	// Show Daemon Usage
	if (argc < 2 || strcmp(argv[1], "--help") == 0) UsageError(argv[0]);
	
	// If Daemon is already running, exit as failure
	IsAlreadyRunning(argv[0]);

	// Run Daemon in Background
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

	// Listen for Termination Signal
	struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = HandleSignal;
    sigaction(SIGTERM, &action, NULL);

	// Check validity of arguments as folders
	int i;
	for (i = 1; i < argc; i++) {
		if (!IsDirectory(argv[i])) ErrorExit("Only Directories can be Watched!\n");
		if (argv[i][strlen(argv[i]) - 1] == '/') argv[i][strlen(argv[i]) - 1] = '\0';
	}

	struct sockaddr_in server_addr;
	int server_socket, server_addr_size, port = 8888;
	char ip[INET_ADDRSTRLEN];
	int len;

	client_socket = socket(AF_INET,SOCK_STREAM,0);
	memset(server_addr.sin_zero,'\0',sizeof(server_addr.sin_zero));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	char log[1024];
	snprintf(log, 1024, "%sdaemon-log-%s.log", LOG_FOLDER, GetTimestamp());
	LogFD = fopen(log, "w");

	if(connect(client_socket,(struct sockaddr *)&server_addr,sizeof(server_addr)) < 0)
		ErrorExit("Connection not established");
	inet_ntop(AF_INET, (struct sockaddr *)&server_addr, ip, INET_ADDRSTRLEN);
	LogMessage("Connected to %s",ip);

	int wd;
	char buffer[BUF_LEN];
	ssize_t bytes_read;
	struct inotify_event *event;

	inotify_fd = inotify_init();
	if (inotify_fd == -1) ErrorExit("inotify_init");

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
		if (IsDirectory(path)) AddWatch(path);
	}
	else if (event->mask & IN_DELETE_SELF) StopWatch(event->wd);
	else if (event->mask & IN_MOVED_TO && event->mask & IN_ISDIR) {
		char path[PATH_MAX + NAME_MAX];
		snprintf(path, sizeof(path), "%s/%s", wd_path, event->name);
		if (IsDirectory(path)) AddWatch(path);
	}
	LogEvent(event);
}

void HandleSignal(int signal) {
	if (signal == SIGTERM) keep_going = 0;
}

void LogEvent(struct inotify_event *i) {
	char buffer[1024], buffer1[128], buffer2[PATH_MAX];
	snprintf(buffer, 1024, "wd =%2d; ", i->wd);
	if (i->cookie > 0) snprintf(buffer1, 128, "cookie =%4d; ", i->cookie);
	strcat(buffer, buffer1);
	strcat(buffer, "mask =");
	if (i->mask & IN_ACCESS) 		strcat(buffer, " IN_ACCESS");
	if (i->mask & IN_ATTRIB) 		strcat(buffer, " IN_ATTRIB");
	if (i->mask & IN_CLOSE_NOWRITE) strcat(buffer, " IN_CLOSE_NOWRITE");
	if (i->mask & IN_CLOSE_WRITE)	strcat(buffer, " IN_CLOSE_WRITE");
	if (i->mask & IN_CREATE) 		strcat(buffer, " IN_CREATE");
	if (i->mask & IN_DELETE) 		strcat(buffer, " IN_DELETE");
	if (i->mask & IN_DELETE_SELF) 	strcat(buffer, " IN_DELETE_SELF");
	if (i->mask & IN_IGNORED) 		strcat(buffer, " IN_IGNORED");
	if (i->mask & IN_ISDIR) 		strcat(buffer, " IN_ISDIR");
	if (i->mask & IN_MODIFY) 		strcat(buffer, " IN_MODIFY");
	if (i->mask & IN_MOVE_SELF) 	strcat(buffer, " IN_MOVE_SELF");
	if (i->mask & IN_MOVED_FROM) 	strcat(buffer, " IN_MOVED_FROM");
	if (i->mask & IN_MOVED_TO) 		strcat(buffer, " IN_MOVED_TO");
	if (i->mask & IN_OPEN) 			strcat(buffer, " IN_OPEN");
	if (i->mask & IN_Q_OVERFLOW) 	strcat(buffer, " IN_Q_OVERFLOW");
	if (i->mask & IN_UNMOUNT) 		strcat(buffer, " IN_UNMOUNT");
	strcat(buffer, "; ");
	if (i->len > 0) snprintf(buffer2, PATH_MAX, "name = %s;", i->name);
	strcat(buffer, buffer2);
	LogMessage(buffer);
	/*fprintf(LogFD, "%s -- wd =%2d; ", GetTimestamp(), i->wd);
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
	fprintf(LogFD, "\n");*/
}

void LogMessage(const char* format, ...) {
	char buffer[MSG_LENGTH];
    va_list args;
    va_start(args, format);
    int rc = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
	fprintf(LogFD, "%s -- << \"%s\" >>\n", GetTimestamp(), buffer);
	//int len = send(client_socket,buffer,strlen(buffer), 0);
	int len = write(client_socket, buffer,strlen(buffer));
	if(len < 0) ErrorExit("message not sent");
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
    for (i = 0; i < size_watch_list; i++)
        if (WatchList[i].wd <= 0)
            return i;
	size_watch_list++;
	WatchList = realloc(WatchList, size_watch_list * sizeof(node));
	return size_watch_list-1;
}

bool IsBeingWatched(const char* path) {
	int i;
	for(i = 0; i < size_watch_list; i++)	
		if (strcmp(path, WatchList[i].path) == 0)
			return true;
	return false;
}

node NodeFromWD(uint32_t wd)
{
	int i;
	for(i = 0; i < size_watch_list; i++)	
		if (WatchList[i].wd == wd)
			return WatchList[i];
	return Zero;
}

node NodeFromPath(const char* path)
{
	int i;
	for(i = 0; i < size_watch_list; i++)	
		if (strcmp(path, WatchList[i].path) == 0)
			return WatchList[i];
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
	WatchList[i].wd = wd;
	strncpy(WatchList[i].path, path, PATH_MAX);
	return WatchList[i];
}

void DeleteNode(uint32_t wd, const char *path)
{
	int i;
    for (i = 0; i < size_watch_list; i++) if (WatchList[i].wd == wd) WatchList[i] = Zero;
	size_watch_list--;
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
