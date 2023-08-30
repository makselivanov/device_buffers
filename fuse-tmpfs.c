#define FUSE_USE_VERSION 26

#include <stdlib.h>
#include <fuse.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include "src/storage/storage.h"

storage_t *storage = NULL;
FILE *fd = NULL;

// https://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html
static int do_getattr(const char *path, struct stat *st) {
    fprintf(fd, "getattr with path=\"%s\"\n", path);
    fflush(fd);

    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_atime = time(NULL);
    st->st_mtime = time(NULL);

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    }

    catalog_node_t *node = catalog_get(storage, path);

    if (node == NULL) {
        return -ENOENT;
    }

    size_t id = node->inode_index;
    if (id != -1) {
        inode_t *child = get(storage->inodes, id);
        *st = child->_stat;
        return EXIT_SUCCESS;
    }

    return -ENODATA;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    fprintf(fd, "readdir with path=\"%s\", offset=%ld\n", path, offset);
    fflush(fd);

    int node_id = fi->fh;
    if (node_id == -1) {
        return -EBADF;
    }

    catalog_node_t *node = catalog_get(storage, path);

    if (!node->is_dir) {
        return -ENOTDIR;
    }

    if (filler(buffer, ".", NULL, 0) != 0) {
        return -ENOMEM;
    }

    if (filler(buffer, "..", NULL, 0) != 0) {
        return -ENOMEM;
    }

    for (size_t i = 2; i < node->entries_count; ++i) {
        catalog_node_t *child = get(node->entries, i); //FIXME
        if (filler(buffer, child->fname, NULL, 0) != 0) {
            return -ENOMEM;
        }
    }

    return EXIT_SUCCESS;
}

static int do_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    fprintf(fd, "read with path=\"%s\", size=%ld ,offset=%ld\n", path, size, offset);
    fflush(fd);

    size_t id = fi->fh;
    inode_t *inode = get(storage->inodes, id);
    if (id == -1 || inode->open == 0) {
        return -EBADF;
    }
    return iread(inode, buffer, size, offset);
}

static int do_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    fprintf(fd, "write with path=\"%s\", size=%ld, offset=%ld\n", path, size, offset);
    fflush(fd);

    size_t id = fi->fh;
    inode_t *inode = get(storage->inodes, id);
    if (id == -1 || inode->open == 0) {
        return -EBADF;
    }
    return iwrite(inode, buffer, size, offset);
}

static int do_mkdir(const char *path, mode_t mode) {
    fprintf(fd, "mkdir with path=\"%s\", mode=0%3o\n", path, mode);
    fflush(fd);

    if (!catalog_exists(storage, path)) {
        size_t id = icreate(storage);
        inode_t *inode = get(storage->inodes, id);
        inode->_stat.st_mode = mode | S_IFDIR;
        inode->_stat.st_gid = fuse_get_context()->gid;
        inode->_stat.st_uid = fuse_get_context()->uid;
        inode->open = 0;
        catalog_add(storage, path, true, id);
        return 0;
    }
    return -EEXIST;
}

static int do_mknod(const char *path, mode_t mode, dev_t dev) {
    fprintf(fd, "mknod with path=\"%s\", mode=0%3o, dev=%ld\n", path, mode, dev);
    fflush(fd);

    if (!catalog_exists(storage, path)) {
        size_t id = icreate(storage);
        inode_t *inode = get(storage->inodes, id);
        inode->_stat.st_mode = mode | S_IFREG;
        inode->_stat.st_gid = fuse_get_context()->gid;
        inode->_stat.st_uid = fuse_get_context()->uid;
        inode->open = 0;
        catalog_add(storage, path, false, id);
        return 0;
    }
    return -EEXIST;
}

static int do_opendir(const char *path, struct fuse_file_info *fi) {
    fprintf(fd, "opendir with path=\"%s\"\n", path);
    fflush(fd);

    catalog_node_t *node = catalog_get(storage, path);
    if (node == NULL || node->inode_index == -1) {
        return -ENOENT;
    }

    inode_t *inode = get(storage->inodes, node->inode_index);
    if (!(inode->_stat.st_mode & S_IFDIR)) {
        return -ENOTDIR;
    }

    inode->open++;
    fi->fh = node->inode_index;

    return 0;
}

static int do_releasedir(const char *path, struct fuse_file_info *fi) {
    fprintf(fd, "releasedir with path=\"%s\"\n", path);
    fflush(fd);

    int node = fi->fh;
    inode_t *inode = get(storage->inodes, node);
    if (node == -1 || inode == NULL) {
        return -ENOENT;
    }
    if (!(inode->_stat.st_mode & S_IFDIR)) {
        return -ENOTDIR;
    }

    if (inode->open == 0) {
        return -EBADF;
    }
    --inode->open;
    return 0;
}

static int do_unlink(const char *path) {
    fprintf(fd, "unlink with path=\"%s\"\n", path);
    fflush(fd);

    catalog_node_t *node = catalog_get(storage, path);

    if (node == NULL) {
        return -EEXIST;
    }
    if (node->is_dir) {
        return -EISDIR;
    }
    inode_t* inode = get(storage->inodes, node->inode_index);
    if (inode == NULL) {
        return -EEXIST;
    }
    if (inode->open > 0) {
        return -EBUSY;
    }
    catalog_erase(node, true);
    return 0;
}

static int do_rmdir(const char *path) {
    fprintf(fd, "rmdir with path=\"%s\"\n", path);
    fflush(fd);

    catalog_node_t *node = catalog_get(storage, path);

    if (node == NULL) {
        return -ENOENT;
    }
    if (!node->is_dir) {
        return -ENOTDIR;
    }
    inode_t* inode = get(storage->inodes, node->inode_index);
    if (inode == NULL) {
        return -EEXIST;
    }
    if (inode->open > 0) {
        return -EBUSY;
    }
    if (node->entries_count > 0) {
        return -ENOTEMPTY;
    }
    catalog_erase(node, true);
    return 0;
}

static int do_open(const char *path, struct fuse_file_info *fi) {
    fprintf(fd, "open with path=\"%s\"\n", path);
    fflush(fd);

    catalog_node_t *node = catalog_get(storage, path);
    if (node == NULL || node->inode_index == -1) {
        return -ENOENT;
    }
    inode_t *inode = get(storage->inodes, node->inode_index);

    if (inode->_stat.st_mode & S_IFDIR) {
        return -EISDIR;
    }

    inode->open++;
    fi->fh = node->inode_index;
    return 0;
}

static int do_release(const char *path, struct fuse_file_info *fi) {
    fprintf(fd, "release with path=\"%s\"\n", path);
    fflush(fd);

    if (fi == NULL) {
        return -EBADF;
    }

    inode_t *inode = get(storage->inodes, fi->fh);
    if ((inode->_stat.st_mode & S_IFDIR)) {
        return -EISDIR;
    }

    if (inode->open == 0) {
        return -EBADF;
    }

    inode->open--;
    return 0;
}

static int do_truncate(const char *path, off_t newsize) {
    fprintf(fd, "release with path=\"%s\" and new size=%ld\n", path, newsize);
    fflush(fd);

    catalog_node_t *node = catalog_get(storage, path);
    size_t node_id = -1;
    if (node == NULL) {
        node_id = icreate(storage);
        //inode_t *inode = get(storage->inodes, node_id);
        //FIXME Add path?
    } else if (node->is_dir) {
        return -EISDIR;
    } else {
        node_id = node->inode_index;
    }

    inode_t *inode = get(storage->inodes, node_id);
    if (inode->capacity < newsize) {
        char *buffer = calloc(newsize, sizeof(char));
        memcpy(buffer, inode->data, inode->capacity);
        free(inode->data);
        inode->data = buffer;
    }
    inode->capacity = newsize;
    return 0;
}

static struct fuse_operations operations = {
        .getattr  = do_getattr,
        .mknod    = do_mknod,
        .mkdir    = do_mkdir,

        .read     = do_read,
        .write    = do_write,
        .open = do_open,
        .release = do_release,
        .truncate = do_truncate,

        .opendir  = do_opendir,
        .releasedir = do_releasedir,
        .unlink = do_unlink,
        .readdir  = do_readdir,
        .rmdir = do_rmdir,
};

int main(int argc, char *argv[]) {
    fd = fopen("log.txt", "w+");
    storage = init_storage();
    int result = fuse_main(argc, argv, &operations, NULL);
    free_storage(storage);
    return result;
}