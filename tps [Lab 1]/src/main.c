#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    DIR *directory;
    struct dirent *file;

    directory = opendir("/proc");
    if (directory == NULL)
    {
        perror("Error: ");
        exit(errno);
    }

    printf("  PID CMD\n");

    while((file = readdir(directory)) != NULL)
    {
        if(isdigit(file->d_name[0]))
        {
            char path[256];
            strcpy(path, "/proc/");
            strcat(path, file->d_name);
            strcat(path, "/status");
            FILE * status = fopen(path, "r");
            if (status == NULL) {
                if (errno == ENOENT) {
                    continue;
                } else {
                    perror("Error: ");
                    exit(errno);
                }
            }

            char fileline[256];
            char filename[256] = "Error: name not found";

            while (fgets(fileline, sizeof(fileline), status)) {
                if (strncmp(fileline, "Name:", 5) == 0) {
                    char *start = fileline + 6;
                    char *n = strchr(start, '\n');
                    if (n != NULL) {
                        *n = '\0';
                    }
                    strncpy(filename, start, sizeof(filename) - 1);
                    break;
                }
            }
            
            fclose(status);
            printf("%5s %s\n", file->d_name, filename);
        }
    } 
    closedir(directory);
    return 0;
};
