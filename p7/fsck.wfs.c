/**
 * This program compacts the log by removing redundancies.
 * The disk_path is given as its argument,
 * i.e., fsck disk_path. This functionality is exclusively for earning bonus points.
 */
#include "wfs.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void compact_entries(void *disk_map, size_t disk_size, struct wfs_sb *sb)
{
    size_t read_offset = sizeof(struct wfs_sb);
    size_t write_offset = read_offset;
    struct wfs_log_entry *read_entry;
    struct wfs_log_entry *write_entry;

    while (read_offset < disk_size)
    {
        read_entry = (struct wfs_log_entry *)((char *)disk_map + read_offset);
        if (read_entry->inode.deleted == 0)
        { // If entry is not deleted
            if (read_offset != write_offset)
            {
                // Move the entry to write_offset
                write_entry = (struct wfs_log_entry *)((char *)disk_map + write_offset);
                memcpy(write_entry, read_entry, sizeof(struct wfs_log_entry)); // Adjust the size as necessary
                write_offset += sizeof(struct wfs_log_entry);
            }
            else
            {
                write_offset = read_offset + sizeof(struct wfs_log_entry);
            }
        }
        read_offset += sizeof(struct wfs_log_entry);
    }

    // Update the head pointer in the superblock
    sb->head = write_offset;
}

void fsck_wfs(const char *disk_path)
{
    int fd;
    struct wfs_sb *sb;
    size_t disk_size = 1048576; // Assuming 1MB disk size

    // Open the disk file and map it to memory
    fd = open(disk_path, O_RDWR);
    if (fd == -1)
    {
        perror("Error opening disk file");
        exit(EXIT_FAILURE);
    }

    void *disk_map = mmap(NULL, disk_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk_map == MAP_FAILED)
    {
        perror("Error mapping disk file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Start from the superblock
    sb = (struct wfs_sb *)disk_map;

    // Compact the entries
    compact_entries(disk_map, disk_size, sb);

    // Synchronize changes to disk
    if (msync(disk_map, disk_size, MS_SYNC) == -1)
    {
        perror("Error syncing memory to disk");
    }

    // Clean up
    munmap(disk_map, disk_size);
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <disk_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    fsck_wfs(argv[1]);

    return 0;
}
