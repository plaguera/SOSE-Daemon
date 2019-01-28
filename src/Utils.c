#include "Utils.h"

char time_buffer[26];

void IsAlreadyRunning(const char* process)
{
	int pid = GetPIDbyName(process);
	if (pid >= 0)
		ErrorExit("'%s' is already running in the background!. PID = %d\n", process, pid);
}

void KillProcess(const char* process)
{
	int pid = GetPIDbyName(process);
	if (pid >= 0) {
		kill(pid, SIGTERM);
		printf("Killed '%s'!. PID = %d\n", process, pid);
		exit(EXIT_SUCCESS);
	}
	ErrorExit("'%s' is not running!\n", process);
}

void ErrorExit(const char* format, ...)
{
	char buffer[4096];
    va_list args;
    va_start(args, format);
    int rc = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
	perror(buffer);
	exit(EXIT_FAILURE);
}

int IsDirectory(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

int GetPIDbyName(const char* name)
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];
    
    if (!(dir = opendir("/proc"))) ErrorExit("can't open /proc");

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
