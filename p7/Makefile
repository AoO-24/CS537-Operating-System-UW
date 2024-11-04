NAME = mount.wfs mkfs.wfs fsck.wfs

CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18 -D_FILE_OFFSET_BITS=64
FUSE_CFLAGS = `pkg-config fuse --cflags --libs`

.PHONY: all
all: $(NAME)

.PHONY: mount.wfs
mount.wfs:
	$(CC) $(CFLAGS) mount.wfs.c $(FUSE_CFLAGS) -o mount.wfs

.PHONY: mkfs.wfs
mkfs.wfs:
	$(CC) $(CFLAGS) -o mkfs.wfs mkfs.wfs.c

.PHONY: fsck.wfs
fsck.wfs:
	$(CC) $(CFLAGS) -o fsck.wfs fsck.wfs.c

.PHONY: clean
clean:
	rm -rf $(NAME)
