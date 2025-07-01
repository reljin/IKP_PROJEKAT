#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

Node* createNode(void* value) {
    Node* newNode = (Node*)malloc(sizeof(Node)); 
    if (newNode == NULL) {
        printf("Greska: Nema dovoljno memorije!\n");
        return NULL;
    }
  
    newNode->data = value;
    newNode->next = NULL;
    return newNode;
}

void insertAtEnd(Node** head, void* value, size_t dataSize) {
    Node* newNode = createNode(value);
    if (newNode == NULL)
        return;

    if (*head == NULL) {  
        *head = newNode;
        return;
    }

    Node* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newNode;
}


void insertAtBeginning(Node** head, void* value, size_t dataSize) {
    Node* newNode = createNode(value);
    if (newNode == NULL)
        return;

    newNode->next = *head;
    *head = newNode;
}


void deleteNode(Node** head, void* data, size_t dataSize, int(*compare)(void*, void*)) {
    Node* current = *head;
    Node* prev = NULL;

    while (current != NULL) {
        if (compare(current->data, data) == 0) {
            if (prev == NULL) {
                *head = current->next;
            }
            else {
                prev->next = current->next;
            }

            free(current->data); // bitno da oslobodiš Message
            free(current);       // oslobodiš čvor
            return;
        }

        prev = current;
        current = current->next;
    }
}


size_t getListSize(Node* head) {
    size_t size = 0;
    Node* current = head;
    while (current != NULL) {
        size++;
        current = current->next;
    }
    return size;
}

// Funkcija za prikaz elemenata u listi
void displayList(Node* head, void (*printData)(void*)) {
    if (head == NULL) {
        printf("Lista je prazna!\n");
        return;
    }
    Node* temp = head;
    printf("Elementi u listi: ");
    while (temp != NULL) {
        printData(temp->data);
        temp = temp->next;
    }
    printf("\n");
}