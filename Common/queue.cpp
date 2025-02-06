#include "queue.h"

// Funkcija za kreiranje reda
Queue* createQueue(size_t dataSize) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    if (!queue) {
        perror("Greška pri alokaciji memorije za red");
        exit(EXIT_FAILURE);
    }
    queue->front = queue->rear = NULL;
    queue->dataSize = dataSize;
    return queue;
}

// Dodavanje elementa u red (enqueue)
void enqueue(Queue* queue, void* data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (!newNode) {
        perror("Greška pri alokaciji memorije za čvor");
        exit(EXIT_FAILURE);
    }

    newNode->data = malloc(queue->dataSize);
    if (!newNode->data) {
        perror("Greška pri alokaciji memorije za podatke");
        free(newNode);
        exit(EXIT_FAILURE);
    }
    memcpy(newNode->data, data, queue->dataSize); // Kopiranje podataka
    newNode->next = NULL;

    // Ako je red prazan
    if (queue->rear == NULL) {
        queue->front = queue->rear = newNode;
        return;
    }

    // Dodavanje na kraj reda
    queue->rear->next = newNode;
    queue->rear = newNode;
}

// Uklanjanje elementa iz reda (dequeue)
void dequeue(Queue* queue, void* data) {
    if (queue->front == NULL) {
        printf("Red je prazan!\n");
        return;
    }

    // Kopiramo podatke iz prvog elementa (ako data nije NULL)
    if (data) {
        memcpy(data, queue->front->data, queue->dataSize);
    }

    // Uklanjanje prvog elementa
    Node* temp = queue->front;
    queue->front = queue->front->next;

    // Ako je red sada prazan, rear treba postaviti na NULL
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(temp->data);
    free(temp);
}

// Pogled na prvi element (bez uklanjanja)
void peek(Queue* queue, void* data) {
    if (queue->front == NULL) {
        printf("Red je prazan!\n");
        return;
    }

    memcpy(data, queue->front->data, queue->dataSize);
}

// Provera da li je red prazan
int isEmpty(Queue* queue) {
    return queue->front == NULL;
}

// Oslobađanje reda
void freeQueue(Queue* queue) {
    while (!isEmpty(queue)) {
        void* temp = malloc(queue->dataSize);
        dequeue(queue, temp);
        free(temp);
    }
    free(queue);
}
