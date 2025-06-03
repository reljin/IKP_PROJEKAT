#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

// Funkcija za kreiranje novog čvora koja čuva originalni pokazivač
Node* createNode(void* value) {
    Node* newNode = (Node*)malloc(sizeof(Node)); //zauzeta 
    if (newNode == NULL) {
        printf("Greška: Nema dovoljno memorije!\n");
        return NULL;
    }
    // Umesto kopiranja, samo dodeljujemo pokazivač
    newNode->data = value;
    newNode->next = NULL;
    return newNode;
}

// Funkcija za umetanje elementa na kraj liste
// Parametar dataSize ostaje radi kompatibilnosti, ali se ne koristi
void insertAtEnd(Node** head, void* value, size_t dataSize) {
    Node* newNode = createNode(value);
    if (newNode == NULL)
        return;

    if (*head == NULL) {  // Ako je lista prazna
        *head = newNode;
        return;
    }

    Node* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newNode;
}

// Funkcija za umetanje elementa na početak liste
void insertAtBeginning(Node** head, void* value, size_t dataSize) {
    Node* newNode = createNode(value);
    if (newNode == NULL)
        return;

    newNode->next = *head;
    *head = newNode;
}

// Funkcija za brisanje čvora sa zadatom vrednošću (pretraga po podacima (posledji argument je za prosledjivanje f-je dal trazim po id-u ili po pokazivacu))
// Ova verzija ne oslobadja memoriju podataka (newNode->data) jer lista samo čuva originalni pokazivač.
void deleteNode(Node** head, void* value, size_t dataSize, int (*cmp)(void*, void*)) {
    if (*head == NULL) {
        printf("Lista je prazna!\n");
        return;
    }

    Node* temp = *head;
    if (cmp(temp->data, value) == 0) {  // Ako je prvi čvor taj koji treba obrisati
        *head = temp->next;
        free(temp);
        return;
    }

    Node* prev = NULL;
    while (temp != NULL && cmp(temp->data, value) != 0) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) {  // Ako element nije pronađen
        printf("Element nije pronadjen u listi!\n");
        return;
    }

    prev->next = temp->next;
    free(temp);
}

// Funkcija za vraćanje veličine liste
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