#ifndef HW_LINKED_LIST_H
#define HW_LINKED_LIST_H

typedef struct node_t {
    struct node_t *next;
    struct node_t *prev;
    void *data;
} node_t;

typedef struct linked_list_t {
    node_t *head;
    size_t size;
} linked_list;

linked_list init_llist();
void push_back(linked_list llist, void *data);
void *get(linked_list llist, size_t index);
void *remove(linked_list llist, size_t index);
void free_list(linked_list llist);

#endif //HW_LINKED_LIST_H
