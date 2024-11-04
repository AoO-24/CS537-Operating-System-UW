#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include "wfs.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

#define MAX_PATH_LENGTH 128

void *disk = NULL;
struct wfs_sb *sb = NULL;

struct wfs_log_entry *get_log_entry(unsigned int ino)
{
    char *log = (char *)((char *)disk + sizeof(struct wfs_sb));
    struct wfs_log_entry *res = NULL;
    while (log < (char *)disk + sb->head)
    {
        if (((struct wfs_log_entry *)log)->inode.inode_number == ino && ((struct wfs_log_entry *)log)->inode.deleted == 0)
        {
            res = (struct wfs_log_entry *)log;
        }
        log += (sizeof(struct wfs_inode) + ((struct wfs_log_entry *)log)->inode.size);
    }
    return res;
}

unsigned int Max_InodeNum()
{
    char *currentPtr = (char *)((char *)disk + sizeof(struct wfs_sb));
    struct wfs_log_entry *currentLogEntry = (struct wfs_log_entry *)currentPtr;
    unsigned int maxInodeNum = 0;

    while (currentPtr < (char *)disk + sb->head)
    {
        currentLogEntry = (struct wfs_log_entry *)currentPtr;
        if (currentLogEntry->inode.inode_number > maxInodeNum && !currentLogEntry->inode.deleted)
        {
            maxInodeNum = currentLogEntry->inode.inode_number;
        }
        currentPtr += sizeof(struct wfs_inode) + currentLogEntry->inode.size;
    }
    return maxInodeNum;
}

unsigned long get_inode_number(char *name, struct wfs_log_entry *logEntry)
{
    char *endPtr = (char *)logEntry + sizeof(struct wfs_inode) + logEntry->inode.size;
    for (char *currentPtr = logEntry->data; currentPtr < endPtr; currentPtr += sizeof(struct wfs_dentry))
    {
        struct wfs_dentry *dentry = (struct wfs_dentry *)currentPtr;
        if (strcmp(name, dentry->name) == 0)
        {
            return dentry->inode_number;
        }
    }
    return (unsigned long)-1;
}

struct wfs_log_entry *path_to_log_entry(const char *path)
{
    char *pathCopy = strdup(path);
    if (!pathCopy)
    {
        return NULL;
    }

    unsigned long inodeNum = 0;
    struct wfs_log_entry *currentLogEntry = get_log_entry(inodeNum);
    char *token = strtok(pathCopy, "/");

    while (token)
    {
        if (!currentLogEntry)
        {
            free(pathCopy);
            return NULL;
        }
        inodeNum = get_inode_number(token, currentLogEntry);
        if (inodeNum == (unsigned long)-1)
        {
            free(pathCopy);
            return NULL;
        }

        currentLogEntry = get_log_entry(inodeNum);
        token = strtok(NULL, "/");
    }
    free(pathCopy);
    return currentLogEntry;
}

struct wfs_log_entry *find_parent_log_path(const char *path)
{
    if (!path)
    {
        return NULL;
    }

    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0)
    {
        // is root
        return get_log_entry(0);
    }

    char parentPath[MAX_PATH_LENGTH];
    strncpy(parentPath, path, sizeof(parentPath) - 1);
    parentPath[sizeof(parentPath) - 1] = '\0';

    char *slashPosition = strrchr(parentPath, '/');
    if (!slashPosition || slashPosition == parentPath)
    {
        return get_log_entry(0);
    }

    *slashPosition = '\0';
    return path_to_log_entry(parentPath);
}

static int my_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));

    struct wfs_log_entry *log = path_to_log_entry(path);
    if (log == NULL)
    {
        // File/directory does not exist while trying to read/write a file/directory
        return -ENOENT;
    }
    stbuf->st_size = log->inode.size;
    stbuf->st_nlink = log->inode.links;
    stbuf->st_mode = log->inode.mode;
    stbuf->st_mtime = log->inode.mtime;
    stbuf->st_gid = log->inode.gid;
    stbuf->st_uid = log->inode.uid;

    return 0;
}

int my_mknod(const char *path, mode_t mode, dev_t dev)
{
    if (path == NULL)
    {
        // Invalid
        return -EINVAL;
    }
    const char *pos = strrchr(path, '/');
    const char *fileName;
    if (pos != NULL)
    {
        fileName = pos + 1;
    }
    else // No '/' found
    {
        fileName = path;
    }

    struct wfs_log_entry *parent_log = find_parent_log_path(path);
    unsigned int num = Max_InodeNum() + 1;

    if (get_inode_number((char *)fileName, parent_log) != -1)
    {
        // File already exists
        return -EEXIST;
    }
    // append a new parent log
    struct wfs_log_entry *newP = (struct wfs_log_entry *)((char *)disk + sb->head);

    memcpy((char *)newP, (char *)parent_log, sizeof(struct wfs_log_entry) + parent_log->inode.size);

    // append a new dentry right under the parent log
    struct wfs_dentry *newD = (void *)((char *)newP->data + newP->inode.size);

    newD->inode_number = num;
    strcpy(newD->name, fileName);
    newP->inode.size = newP->inode.size + sizeof(struct wfs_dentry);
    sb->head = sb->head + (uint32_t)(sizeof(struct wfs_inode) + newP->inode.size);

    // append new new file log
    struct wfs_log_entry *newF = (struct wfs_log_entry *)((char *)disk + sb->head);
    newF->inode.inode_number = num;
    newF->inode.size = 0;
    newF->inode.links = 1;
    newF->inode.mode = mode | S_IFREG;
    newF->inode.flags = 0;
    newF->inode.deleted = 0;
    unsigned int uid = getuid();
    newF->inode.gid = uid;
    newF->inode.uid = uid;
    unsigned t = (unsigned)time(NULL);
    newF->inode.atime = t;
    newF->inode.mtime = t;
    newF->inode.ctime = t;
    sb->head = sb->head + (uint32_t)(sizeof(struct wfs_inode));
    return 0;
}

int my_mkdir(const char *path, mode_t mode)
{
    if (path == NULL)
    {
        // Invalid
        return -EINVAL;
    }

    // Find the last '/'
    const char *pos = strrchr(path, '/');
    const char *fileName;
    if (pos != NULL)
    {
        fileName = pos + 1;
    }
    else
    {
        fileName = path;
    }

    struct wfs_log_entry *parent_log = find_parent_log_path(path);
    unsigned int num = Max_InodeNum() + 1;

    if (get_inode_number((char *)fileName, parent_log) != -1)
    {
        // already exists
        return -EEXIST;
    }

    // append a new parent log
    struct wfs_log_entry *newP = (struct wfs_log_entry *)((char *)disk + sb->head);
    memcpy((char *)newP, (char *)parent_log, sizeof(struct wfs_log_entry) + parent_log->inode.size);
    // append a new dentry right under the parent log
    struct wfs_dentry *newD = (void *)((char *)newP->data + newP->inode.size);

    newD->inode_number = num;
    strcpy(newD->name, fileName);
    newP->inode.size = newP->inode.size + sizeof(struct wfs_dentry);
    sb->head = sb->head + (uint32_t)(sizeof(struct wfs_inode) + newP->inode.size);
    // append new new file log
    struct wfs_log_entry *newF = (struct wfs_log_entry *)((char *)disk + sb->head);
    newF->inode.inode_number = num;
    newF->inode.mode = S_IFDIR | mode;
    newF->inode.links = 1;
    newF->inode.deleted = 0;
    newF->inode.size = 0;
    newF->inode.flags = 0;
    unsigned t = (unsigned)time(NULL);
    newF->inode.atime = t;
    newF->inode.mtime = t;
    newF->inode.ctime = t;
    unsigned int uid = getuid();
    newF->inode.gid = uid;
    newF->inode.uid = uid;
    sb->head = sb->head + (uint32_t)(sizeof(struct wfs_inode));
    return 0;
}

int my_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    struct wfs_log_entry *log = path_to_log_entry(path);

    if (log == NULL)
    {
        // File/directory does not exist while trying to read/write a file/directory
        return -ENOENT;
    }

    if (offset < log->inode.size)
    {
        memcpy(buf, log->data + offset, size);
        return size;
    }

    // nothing to read
    return 0;
}

int my_write(const char *path, const char *buffer, size_t writeSize, off_t writeOffset, struct fuse_file_info *fileInfo)
{
    if (writeOffset < 0)
    {
        return 0;
    }

    struct wfs_log_entry *currentLogEntry = path_to_log_entry(path);
    if (currentLogEntry == NULL)
    {
        // File/directory does not exist while trying to read/write a file/directory
        return -ENOENT;
    }
    struct wfs_log_entry *newLogEntry = (struct wfs_log_entry *)((char *)disk + sb->head);
    memcpy((char *)newLogEntry, (char *)currentLogEntry, sizeof(struct wfs_inode) + currentLogEntry->inode.size);

    if (writeOffset + writeSize > currentLogEntry->inode.size)
    {
        newLogEntry->inode.size = writeOffset + writeSize;
    }
    memcpy((char *)newLogEntry->data + writeOffset, buffer, writeSize);

    newLogEntry->inode.mtime = time(NULL);

    sb->head += (uint32_t)(sizeof(struct wfs_inode)) + newLogEntry->inode.size;

    return writeSize;
}

int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    struct wfs_log_entry *currentLog = path_to_log_entry(path);
    struct wfs_log_entry *parentLog = find_parent_log_path(path);

    if (!currentLog || !parentLog)
    {
        // File/directory does not exist while trying to read/write a file/directory
        return -ENOENT;
    }

    struct stat statBuffer;

    // check current entry
    memset(&statBuffer, 0, sizeof(statBuffer));
    statBuffer.st_ino = currentLog->inode.inode_number;
    statBuffer.st_mode = currentLog->inode.mode;
    if (filler(buf, ".", &statBuffer, 0) != 0)
    {
        return 0;
    }

    // check parent entry
    memset(&statBuffer, 0, sizeof(statBuffer));
    statBuffer.st_ino = parentLog->inode.inode_number;
    statBuffer.st_mode = parentLog->inode.mode;
    if (filler(buf, "..", &statBuffer, 0) != 0)
    {
        return 0;
    }
    char *endOfDirectory = (char *)currentLog + sizeof(struct wfs_inode) + currentLog->inode.size;
    for (char *entryPtr = currentLog->data; entryPtr < endOfDirectory; entryPtr += sizeof(struct wfs_dentry))
    {
        struct wfs_dentry *dirEntry = (struct wfs_dentry *)entryPtr;

        if (!get_log_entry(dirEntry->inode_number)->inode.deleted)
        {
            memset(&statBuffer, 0, sizeof(statBuffer));
            statBuffer.st_ino = dirEntry->inode_number;
            statBuffer.st_mode = get_log_entry(dirEntry->inode_number)->inode.mode;

            if (filler(buf, dirEntry->name, &statBuffer, 0) != 0)
            {
                break;
            }
        }
    }

    return 0;
}

static int wfs_unlink(const char *path)
{
    struct wfs_log_entry *file_entry = path_to_log_entry(path);
    if (file_entry == NULL)
    {
        return -ENOENT;
    }

    if ((file_entry->inode.mode & S_IFMT) == S_IFDIR)
    {
        return -EISDIR; // Can't unlink a directory
    }

    file_entry->inode.deleted = 1;
    return 0;
}

static struct fuse_operations my_operations = {
    .getattr = my_getattr,
    .mknod = my_mknod,
    .mkdir = my_mkdir,
    .read = my_read,
    .write = my_write,
    .readdir = my_readdir,
    .unlink = wfs_unlink,
};

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s [FUSE options] disk_path mount_point\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *diskPath = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-f") != 0 && strcmp(argv[i], "-s") != 0)
        {
            diskPath = argv[i];
            // Shift the remaining arguments left by one position
            for (int j = i; j < argc - 1; j++)
            {
                argv[j] = argv[j + 1];
            }
            argc--;
            break;
        }
    }

    // Validate disk path
    if (diskPath == NULL)
    {
        return EXIT_FAILURE;
    }

    // Open the disk image file
    int fd = open(diskPath, O_RDWR | O_CREAT, 0666);
    if (fd == -1)
    {
        return EXIT_FAILURE;
    }
    struct stat diskStat;
    if (fstat(fd, &diskStat) == -1)
    {
        perror("Failed to get disk file size");
        close(fd);
        return EXIT_FAILURE;
    }
    disk = mmap(NULL, diskStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED)
    {
        perror("Failed to map disk file");
        close(fd);
        return EXIT_FAILURE;
    }
    sb = (struct wfs_sb *)disk;

    // Initialize FUSE
    int fuseResult = fuse_main(argc, argv, &my_operations, NULL);
    munmap(disk, diskStat.st_size);
    close(fd);

    return fuseResult;
}