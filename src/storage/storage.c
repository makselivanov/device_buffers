#include "storage.h"
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <libgen.h>

storage_t *init_storage() {
    storage_t *storage = malloc(sizeof(storage_t));
    storage->inodes = init_llist();
    storage->catalog = init_catalog();
    icreate(storage);
    inode_t *root = get(storage->inodes, 0);
    root->_stat.st_mode = S_IFDIR;
    return storage;
}

void free_storage(storage_t *storage) {
    node_t *node = storage->inodes->head->next;
    for (size_t i = 0; i < storage->inodes->size; ++i) {
        inode_t *inode = node->data;
        free(inode->data);
        free(inode);
        node = node->next;
    }
    free_catalog(storage->catalog);
    free_list(storage->inodes);
}


int iwrite(inode_t *inode, const void *data, size_t size, size_t offset) {
    if (offset > inode->_stat.st_size) {
        return -EOVERFLOW;
    }
    if (data == NULL) {
        return -EFAULT;
    }
    if (offset + size >= inode->capacity) {
        void *new_ptr = realloc(inode->data, offset + size);
        if (new_ptr == NULL) {
            return -ENOMEM;
        }
        inode->capacity = offset + size;
        inode->data = new_ptr;
    }
    if (memcpy(inode->data + offset, data, size) == NULL) {
        return -EFAULT;
    }
    if (offset + size > inode->_stat.st_size) {
        inode->_stat.st_size = offset + size;
    }
    return size;
}

int iread(inode_t *inode, void *data, size_t size, size_t offset) {
    if (offset >= inode->_stat.st_size) {
        return 0;
        //return -EOVERFLOW;
    }
    if (offset + size > inode->_stat.st_size) {
        size -= inode->_stat.st_size - offset;
    }
    if (memcpy(data, inode->data + offset, size) == NULL) {
        return -EFAULT;
    }
    return size;
}

unsigned long icreate(storage_t *storage) {
    inode_t *inode = malloc(sizeof(inode_t));
    inode->capacity = 0;
    inode->data = NULL;
    inode->_stat.st_size = 0;
    inode->_stat.st_ino = storage->inodes->size;
    inode->open = 0;
    push_back(storage->inodes, inode);
    return inode->_stat.st_ino;
}


int catalog_add(storage_t *storage, const char *path, bool is_dir, size_t inode_index) {
    if (catalog_exists(storage, path)) {
        return -EEXIST;
    }
    char *base_name_tmp = strdup(path);
    char *dir_path_tmp = strdup(path);
    char *base_name = basename(base_name_tmp);
    char *dir_path = dirname(dir_path_tmp);
    base_name = strdup(base_name);
    dir_path = strdup(dir_path);
    free(base_name_tmp);
    free(dir_path_tmp);

    catalog_node_t *dir = catalog_get(storage, dir_path);
    if (dir != NULL) {
        if (!dir->is_dir) {
            return -ENOTDIR;
        }
        catalog_node_t *node = malloc(sizeof(catalog_node_t));
        catalog_node_t buffer = {
                .is_dir = is_dir,
                .inode_index = inode_index,
                .entries_count = 0,
                .fname = base_name,
                .parent = dir
        };
        *node = buffer;
        if (is_dir) {
            node->entries = init_llist();
            push_back(node->entries, node);
            push_back(node->entries, dir);
            node->entries_count = 2;
        }
        push_back(dir->entries, node);
        dir->entries_count++;
    } else {
        return -EEXIST;
    }
    free(dir_path);
    return 0;
}

catalog_node_t* catalog_get(storage_t *storage, const char *path) {
    char *buffer = strdup(path);
    if (buffer == NULL) {
        return NULL;
    }
    char *fname = strtok(buffer, "/");
    catalog_node_t *current = storage->catalog.root;
    while (fname != NULL) {
        if (!current->is_dir) {
            return NULL;
        }
        bool is_found = false;
        if (strcmp(fname, ".") == 0) {
            is_found = true;
            current = get(current->entries, 0);
        } else if (strcmp(fname, "..") == 0) {
            is_found = true;
            current = get(current->entries, 1);
        } else {
            node_t *cur_node = current->entries->head->next->next->next;
            for (size_t i = 2; i < current->entries_count; ++i) {
                catalog_node_t *next = cur_node->data;
                if (strcmp(fname, next->fname) == 0) {
                    current = next;
                    is_found = true;
                    break;
                }
                cur_node = cur_node->next;
            }
        }

        if (current == NULL || !is_found) {
            return NULL;
        }
        fname = strtok(NULL, "/");
    }
    free(buffer);
    return current;
}

bool catalog_exists(storage_t *storage, const char *path) {
    return (catalog_get(storage, path) != NULL);
}


catalog_t init_catalog() {
    catalog_node_t *root = malloc(sizeof(catalog_node_t));
    catalog_node_t buffer = {
            .is_dir = true,
            .inode_index = 0,
            .fname = "",
            .entries = NULL,
            .entries_count = 0,
    };
    *root = buffer;
    root->entries = init_llist();
    push_back(root->entries, root);
    push_back(root->entries, root);
    root->entries_count = 2;
    catalog_t catalog = {
            .root = root
    };
    return catalog;
}

void free_catalog(catalog_t catalog) {
    catalog_erase(catalog.root, false);
}

void catalog_erase(catalog_node_t *node, bool delete_from_parent) {
    if (node->is_dir) {
        catalog_node_t *current;
        remove_index(node->entries, 0);
        remove_index(node->entries, 0);

        while ((current = remove_index(node->entries, 0)) != NULL) {
            catalog_erase(current, false);
        }

        free_list(node->entries);
    }

    if (delete_from_parent) {
        node_t *cur_node = node->parent->entries->head->next->next->next;
        for (size_t i = 2; i < node->parent->entries_count; ++i) {
            catalog_node_t *next = cur_node->data;
            if (strcmp(next->fname, node->fname) == 0) { //Maybe check ptr?
                remove_index(node->parent->entries, i);
                //free(next); free at the end?
                --node->parent->entries_count;
                break;
            }
            cur_node = cur_node->next;
        }
    }

    free(node);
}