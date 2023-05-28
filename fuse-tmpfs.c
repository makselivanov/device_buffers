#define FUSE_USE_VERSION 26

#include <stdlib.h>
#include <fuse.h>
#include <unistd.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>

struct tmpfs_state {
    char *rootdir;
};

#define TMPFS_DATA ((struct tmpfs_state *) fuse_get_context()->private_data)

// https://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html
static int do_getattr(const char *path, struct stat *st) {
    fprintf(fd, "getattr with path=\"%s\"\n", path);
    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_atime = time(NULL);
    st->st_mtime = time(NULL);

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    }

    node *get_node(root, path);
    if (node == NULL || node->obsolete) {
        return -ENOENT;
    }

    size_t id = node->inode;
    if (id != -1) {
        inode *child = get(st->inodes, id);
        *statbuf = child->_stat;
        return EXIT_SUCCESS;
    }

    return -ENODATA;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    fprintf(fd, "readdir with path=\"%s\", offset=\"%ld\"\n", path, offset);
    int node_id = fi->fh;
    if (node_id == -1) {
        return -EBADF;
    }

    node *get_node(root, path);
    if (node->obsolete) {
        return -EBADF;
    }

    if (!entry->dir) {
        return -ENOTDIR;
    }

    if (filler(buffer, ".", NULL, 0) != 0) {
        return -ENOMEM;
    }

    if (filler(buffer, "..", NULL, 0) != 0) {
        return -ENOMEM;
    }

    for (size_t i = 2; i < node->entries_count; ++i) {
        if (node->entries[i]->obsolete) {
            continue;
        }

        node *child = node->enties[i];
        if (filler(buffer, node->file_name, NULL, 0) != 0) {
            return -ENOMEM;
        }
    }

    return EXIT_SUCCESS;
}

static int do_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    char *content = "moo\n";
    memcpy(buffer, content, (size_t)
    sizeof(content));

    return strlen(content);
}

// TODO: Provide implementation.
static int do_mkdir(const char *path, mode_t mode) {
    mode | S_IFDIR;
    return EXIT_SUCCESS;
}

// TODO: Provide implementation.
static int do_mknod(const char *path, mode_t mode, dev_t rdev) {
    return EXIT_SUCCESS;
}

// TODO: Provide implementation.
static int do_opendir(const char *, struct fuse_file_info *) {
    return EXIT_SUCCESS;
}

// TODO: Provide implementation.
static int do_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *info) {
    return EXIT_SUCCESS;
}

static struct fuse_operations operations = {
        .getattr  = do_getattr,
        .mknod    = do_mknod,
        .mkdir    = do_mkdir,
        .read     = do_read,
        .write    = do_write,
        .opendir  = do_opendir,
        .readdir  = do_readdir,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &operations, NULL);
}