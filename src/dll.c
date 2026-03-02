#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dll.h"

// Creates a new doubly linked list node with the given key and data
Node *create_node(char *key, void *data, size_t size)
{
    Node *node = (Node *)malloc(sizeof(Node));
    if (!node)
        return NULL;

    node->key = strdup(key);
    node->data = data;
    node->size = size;
    node->right = NULL;
    node->left = NULL;
    return node;
}

// Creates a new doubly linked list with a single head node
Node *create_dll(char *headKey, void *data, size_t size)
{
    return create_node(headKey, data, size);
}

// Inserts a new node to the right of the given node
Node *insert_node_right(Node *node, char *key, void *data, size_t size)
{
    if (node == NULL)
        return create_dll(key, data, size);

    Node *new_node = create_node(key, data, size);
    if (!new_node)
        return node;

    if (node->right == NULL)
    {
        node->right = new_node;
        new_node->left = node;

        return node;
    }

    Node *right_node = node->right;

    new_node->right = right_node;
    right_node->left = new_node;

    node->right = new_node;
    new_node->left = node;

    return node;
}

// Inserts a new node to the left of the given node
Node *insert_node_left(Node **head, Node *node, char *key, void *data, size_t size)
{
    if (node == NULL)
        return create_dll(key, data, size);

    Node *new_node = create_node(key, data, size);
    if (!new_node)
        return node;

    if (node->left == NULL)
    {
        node->left = new_node;
        new_node->right = node;

        if (*head == node)
        {
            *head = new_node;
            return new_node;
        }
    }

    Node *left_node = node->left;
    new_node->left = left_node;
    left_node->right = new_node;

    node->left = new_node;
    new_node->right = node;

    return node;
}

// Moves a node to the front of the list (used for LRU cache)
Node *move_to_front(Node **head, Node *node)
{
    if (node == NULL || *head == NULL || *head == node)
    {
        return *head;
    }

    if (node->left != NULL)
    {
        node->left->right = node->right;
    }
    if (node->right != NULL)
    {
        node->right->left = node->left;
    }

    node->left = NULL;
    node->right = *head;
    (*head)->left = node;
    *head = node;

    return *head;
}

// Removes a node from the doubly linked list and frees its memory
void deleteNode(Node **head, Node *node)
{
    if (node == NULL)
    {
        return;
    }

    Node *left_node = node->left;
    Node *right_node = node->right;

    if (*head == node)
    {
        if (right_node != NULL)
            *head = right_node;
        else
            *head = NULL;
    }

    if (left_node != NULL)
    {
        left_node->right = right_node;
        if (right_node != NULL)
        {
            right_node->left = left_node;
        }
    }
    else
    {
        if (right_node != NULL)
        {
            right_node->left = NULL;
        }
    }

    if (node->key)
    {
        free(node->key);
    }
    free(node);
}