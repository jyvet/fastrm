#define _GNU_SOURCE
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

struct linux_dirent {
        long           d_ino;
        off_t          d_off;
        unsigned short d_reclen;
        char           d_name[256];
        char           d_type;
};

static char *path = NULL;
static int totalfiles = 0;
static int totaldir = 0;

static void parse_config(int argc, char **argv)
{
    int option_idx = 0;
    static struct option loptions[] = {
        { "help", no_argument, NULL, 'h' },
        { 0, 0, 0, 0 }
    };

    while (1)
    {
        int c = getopt_long(argc, argv, "h", loptions, &option_idx);
        if (c < 0)
            break;

        switch(c)
        {
            case 0:
                break;

            case 'h':
                printf("Usage: %s DIRECTORY\n"
                       "Delete files in DIRECTORY.\n",
                       argv[0]);
                exit(0);
                break;

            default:
              break;
        }
    }

    if (optind >= argc)
        errx(EXIT_FAILURE, "Must supply a valid directory\n");

    path = argv[optind];
}

int remove_dir_content(const char* dirpath)
{
    int dirfd = -1;
    int bufcount = 0;
    int offset = 0;
    void *buffer = NULL;
    char *d_type;
    struct linux_dirent *dent = NULL;
    struct stat dstat;
    char cwd[PATH_MAX];

    if (lstat(dirpath, &dstat) < 0)
        err(EXIT_FAILURE, "Unable to lstat path");

    if (!S_ISDIR(dstat.st_mode))
        errx(EXIT_FAILURE, "The path %s is not a directory.\n", dirpath);

    /* Allocate a buffer of equal size to the directory to store dents */
    if ((buffer = calloc(dstat.st_size*3, 1)) == NULL)
        err(EXIT_FAILURE, "Buffer allocation failure");

    /* Open the directory */
    if ((dirfd = open(dirpath, O_RDONLY)) < 0)
        err(EXIT_FAILURE, "Open error");

    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
       perror("getcwd() error");
       return 1;
    }

    /* Switch directory */
    fchdir(dirfd);

    while ((bufcount = syscall(SYS_getdents, dirfd, buffer, dstat.st_size * 3)))
    {
        offset = 0;
        dent = buffer;
        while (offset < bufcount)
        {
            if (!((strcmp(".",dent->d_name) == 0) || (strcmp("..",dent->d_name) == 0)))
            {
                d_type = (char *)dent + dent->d_reclen-1;

                if (*d_type == DT_REG)
                {
                    if (unlink(dent->d_name) < 0)
                        warn("Cannot delete file \"%s\"", dent->d_name);

                    totalfiles++;
                }
                else if (*d_type == DT_DIR)
                {
                    /* Recurse */
                    if (remove_dir_content(dent->d_name) != 0)
                        err(EXIT_FAILURE, "Cannot delete files in directory \"%s\"", dent->d_name);
                    else if (rmdir(dent->d_name) < 0)
                        warn("Cannot delete directory \"%s\"", dent->d_name);
                    else
                        totaldir++;
                }
            }
            offset += dent->d_reclen;
            dent = buffer + offset;
        }
    }

    close(dirfd);
    free(buffer);

    if ((dirfd = open(cwd, O_RDONLY)) < 0)
    {
        err(EXIT_FAILURE, "Open error");
        return 1;
    }

    /* Revert directory */
    fchdir(dirfd);
    close(dirfd);
    return 0;
}

int main(int argc, char** argv)
{
    parse_config(argc, argv);

    /* Standard sanity checking stuff */
    if (access(path, R_OK) < 0)
        err(EXIT_FAILURE, "Could not access directory");

    remove_dir_content(path);

    fprintf(stderr, "Total: %d files deleted\n", totalfiles);
    fprintf(stderr, "Total: %d directories deleted\n", totaldir);

    return 0;
}

