#ifndef HW_STORAGE_H
#define HW_STORAGE_H
#include <sys/stat.h>
#include <stdbool.h>
#include <stddef.h>
#include "linked_list.h"

typedef struct inode_t {
    struct stat _stat;
    size_t capacity;
    char *data;
    struct inode_t* parent;
    int open;
} inode_t;

typedef struct catalog_node_t {
    bool is_dir;
    size_t inode_index;
    const char *fname;

    linked_list entries; //contains catalog_node_t of children!! //FIXME??
    size_t entries_count;

    struct catalog_node_t *parent;
} catalog_node_t;

typedef struct catalog_t {
    catalog_node_t *root;
} catalog_t;

typedef struct storage_t {
    linked_list inodes;
    catalog_t catalog;
} storage_t;

storage_t *init_storage();
void free_storage(storage_t *storage);

int iwrite(inode_t *inode, const void *data, size_t size, size_t offset);
int iread(inode_t *inode, void *data, size_t size, size_t offset);
unsigned long icreate(storage_t *storage);

int catalog_add(storage_t *storage, const char *path, bool is_dir, size_t inode_index);
catalog_node_t* catalog_get(storage_t *storage, const char *path);
bool catalog_exists(storage_t *storage, const char *path);

catalog_t init_catalog();
void free_catalog(catalog_t catalog);
void catalog_erase(catalog_node_t *node, bool delete_from_parent);

#endif //HW_STORAGE_H
