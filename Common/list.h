#pragma once
#ifndef LIST_H
#define LIST_H

#include <stddef.h> 
#include <winsock2.h> 

typedef struct Node {
    void* data;         
    struct Node* next;  
} Node;

Node* createNode(void* value, size_t dataSize);  
void insertAtEnd(Node** head, void* value, size_t dataSize);  
void insertAtBeginning(Node** head, void* value, size_t dataSize);  
void deleteNode(Node** head, void* value, size_t dataSize, int (*cmp)(void*, void*));  
size_t getListSize(Node* head);  
void displayList(Node* head, void (*printData)(void*));  

//utils

void insertSocket(Node** head, SOCKET sock);
void removeSocket(Node** head, SOCKET sock);
bool socketExists(Node* head, SOCKET sock);

#endif // LIST_H

