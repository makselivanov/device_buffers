install: fuse-tmpfs.c src/linked_list/linked_list.h src/linked_list/linked_list.c src/storage/storage.h src/storage/storage.c
	gcc fuse-tmpfs.c linked_list.h linked_list.c storage.h storage.c -Wall -D_FILE_OFFSET_BITS=64 -lfuse -o tmpfs

mount: tmpfs
	./tmpfs mountpoint

mount_debug:
	./tmpfs -d mountpoint

unmount:
	umount mountpoint

.PHONY: mount mount_debug unmount

default: install
