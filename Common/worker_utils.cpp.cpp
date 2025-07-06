#include "worker.h"
#include "message.h"
#include "list.h"
#include <mutex>

void removeMessagesFromAllWorkersBySocket(Node* workers, SOCKET sock) {
    Node* current = workers;
    while (current) {
        Worker* w = (Worker*)current->data;
        if (!w) {
            current = current->next;
            continue;
        }

        std::lock_guard<std::mutex> lock(w->mtx);

        int i = 0;
        while (i < w->dataCount) {
            Message* msg = w->data[i];
            if (msg && msg->clientSocket == sock) {
                int msg_id = msg->msg_id;

                free(msg);  
             
                for (int j = i; j < w->dataCount - 1; ++j) {
                    w->data[j] = w->data[j + 1];
                }

                w->data[--w->dataCount] = nullptr;

                removeMessage(w->inflightMessages, msg_id);
            }
            else {
                ++i;
            }
        }

        current = current->next;
    }
}