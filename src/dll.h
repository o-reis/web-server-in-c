#ifndef DLL_H
#define DLL_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>


typedef struct node {
    struct node* left;
    struct node* right;
    char* key;
    void* data;         
    size_t size;
} Node;


Node* create_node(char* key, void* data, size_t size);
Node* create_dll(char* headKey, void* data, size_t size);
Node* insert_node_right(Node* node, char* key, void* data, size_t size);
Node* insert_node_left(Node** head, Node* node, char* key, void* data, size_t size);
void deleteNode(Node** head, Node* node);
Node* move_to_front(Node** head, Node* node);

#endif