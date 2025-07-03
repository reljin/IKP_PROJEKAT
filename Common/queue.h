#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "list.h"


typedef struct Queue {
    Node* front;            
    Node* rear;            
    size_t dataSize;        
} Queue;

Queue* createQueue(size_t dataSize);
void enqueue(Queue* queue, void* data);
void dequeue(Queue* queue, void* data);
void peek(Queue* queue, void* data);
int isEmpty(Queue* queue);
void freeQueue(Queue* queue);
void clearQueue(Queue* queue);

#endif // QUEUE_H
