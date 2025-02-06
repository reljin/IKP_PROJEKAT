#pragma once
#ifndef LIST_H
#define LIST_H

#include <stddef.h> // Za size_t

// Struktura čvora liste
typedef struct Node {
    void* data;         // Podaci čvora
    struct Node* next;  // Pokazivač na sledeći čvor
} Node;

// Funkcije za rad sa listom
Node* createNode(void* value, size_t dataSize);  // Kreira novi čvor sa podacima
void insertAtEnd(Node** head, void* value, size_t dataSize);  // Umeće element na kraj liste
void insertAtBeginning(Node** head, void* value, size_t dataSize);  // Umeće element na početak liste
void deleteNode(Node** head, void* value, size_t dataSize, int (*cmp)(void*, void*));  // Briše čvor iz liste
size_t getListSize(Node* head);  // Vraća veličinu liste
void displayList(Node* head, void (*printData)(void*));  // Prikazuje listu

#endif // LIST_H

