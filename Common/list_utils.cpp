#include "list.h"
#include <winsock2.h>

void insertSocket(Node** head, SOCKET sock) {
    SOCKET* newSock = (SOCKET*)malloc(sizeof(SOCKET));
    *newSock = sock;
    insertAtEnd(head, newSock, sizeof(SOCKET));
}

void removeSocket(Node** head, SOCKET sock) {
    Node* current = *head;
    Node* prev = NULL;

    while (current != NULL) {
        if (*((SOCKET*)current->data) == sock) {
            if (prev == NULL)
                *head = current->next;
            else
                prev->next = current->next;

            free(current->data);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

bool socketExists(Node* head, SOCKET sock) {
    Node* current = head;
    while (current != NULL) {
        if (*((SOCKET*)current->data) == sock)
            return true;
        current = current->next;
    }
    return false;
}