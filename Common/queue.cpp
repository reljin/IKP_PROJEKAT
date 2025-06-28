#include "queue.h"

Queue* createQueue(size_t dataSize) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    if (!queue) {
        perror("Greska pri alokaciji memorije za red");
        exit(EXIT_FAILURE);
    }
    queue->front = queue->rear = NULL;
    queue->dataSize = dataSize;
    return queue;
}

void enqueue(Queue* queue, void* data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) {
        perror("Greska pri alokaciji memorije za cvor");
        exit(EXIT_FAILURE);
    }

    newNode->data = malloc(queue->dataSize);
    if (!newNode->data) {
        perror("Greska pri alokaciji memorije za podatke");
        free(newNode);
        exit(EXIT_FAILURE);
    }
    memcpy(newNode->data, data, queue->dataSize); // Kopiranje podataka
    newNode->next = NULL;

    if (queue->rear == NULL) {
        queue->front = queue->rear = newNode;
        return;
    }

    queue->rear->next = newNode;
    queue->rear = newNode;
}

void dequeue(Queue* queue, void* data) {
    if (queue->front == NULL) {
        printf("Red je prazan!\n");
        return;
    }

  
    if (data) {
        memcpy(data, queue->front->data, queue->dataSize);
    }


    Node* temp = queue->front;
    queue->front = queue->front->next;

    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(temp->data);
    free(temp);
}

void peek(Queue* queue, void* data) {
    if (queue->front == NULL) {
        printf("Red je prazan!\n");
        return;
    }

    memcpy(data, queue->front->data, queue->dataSize);
}

int isEmpty(Queue* queue) {
    return queue->front == NULL;
}

void clearQueue(Queue* queue) {
    while (!isEmpty(queue)) {
        dequeue(queue, NULL);  
    }
}

void freeQueue(Queue* queue) {
    while (!isEmpty(queue)) {
        dequeue(queue, NULL);  
    }
    free(queue);  
}
