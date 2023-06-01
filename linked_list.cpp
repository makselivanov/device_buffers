#include "linked_list.h"
#include <malloc.h>

linked_list init_llist() {
    linked_list list;
    list.head = malloc(sizeof(node_t));
    list.head->next = list.head;
    list.head->prev = list.head;
    list.size = 0;
    return list;
}

void push_back(linked_list llist, void *data) {
    node_t *cur_node = malloc(sizeof(node_t));
    cur_node->data = data;
    cur_node->next = llist.head;
    cur_node->prev = llist.head->prev;
    cur_node->prev->next = cur_node;
    llist.head->prev = cur_node;
    ++llist.size;
}

void *get(linked_list llist, size_t index) {
    if (index >= llist.size)
        return NULL;
    node_t *cur_node = llist.head->next;
    while (index > 0) {
        cur_node = cur_node->next;
        --index;
    }
    return cur_node->data;
}

void *remove(linked_list llist, size_t index) {
    if (index >= llist.size)
        return NULL;
    node_t *cur_node = llist.head->next;
    while (index > 0) {
        cur_node = cur_node->next;
        --index;
    }
    void *data = cur_node->data;
    cur_node->next->prev = cur_node->prev;
    cur_node->prev->next = cur_node->next;
    free(cur_node);
    --llist.size;
    return data;
}

void free_list(linked_list llist) {
    node_t *cur_node = llist.head->next;
    node_t *next_node = NULL;
    for (size_t i = 0; i < llist.size; ++i) {
        next_node = cur_node->next;
        free(cur_node);
        cur_node = next_node;
    }
    free(cur_node); //its llist.head
}