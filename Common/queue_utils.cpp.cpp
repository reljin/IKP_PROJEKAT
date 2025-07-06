#include "queue.h"
#include "message.h"
#include <winsock2.h>

void removeMessagesFromQueueBySocket(Queue* queue, SOCKET sock) {
    if (!queue) return;

    Node* current = queue->front;
    Node* prev = NULL;

    while (current) {
        Message* msg = (Message*)current->data;
        if (msg && msg->clientSocket == sock) {
            Node* toDelete = current;

            if (prev == NULL) {
                queue->front = current->next;
            }
            else {
                prev->next = current->next;
            }

            if (queue->rear == toDelete)
                queue->rear = prev;

            current = current->next;
            free(toDelete->data);
            free(toDelete);
        }
        else {
            prev = current;
            current = current->next;
        }
    }

    if (queue->front == NULL) {
        queue->rear = NULL;
    }
}