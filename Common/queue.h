#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "list.h"

// Struktura za red
typedef struct Queue {
    Node* front;            // Pokazivač na prvi element
    Node* rear;             // Pokazivač na poslednji element
    size_t dataSize;        // Veličina svakog podatka (u bajtovima)
} Queue;

// Funkcija za kreiranje reda
Queue* createQueue(size_t dataSize);

// Funkcija za dodavanje elementa u red (enqueue)
void enqueue(Queue* queue, void* data);

// Funkcija za uklanjanje elementa iz reda (dequeue)
void dequeue(Queue* queue, void* data);

// Funkcija za pogled na prvi element (bez uklanjanja)
void peek(Queue* queue, void* data);

// Funkcija za proveru da li je red prazan
int isEmpty(Queue* queue);

// Funkcija za oslobađanje reda
void freeQueue(Queue* queue);

void clearQueue(Queue* queue);

#endif // QUEUE_H
