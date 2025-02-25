#include "Distributor.h"
#include <iostream>

extern std::mutex clientMessageQueueMutex;
extern std::mutex workersMutex;

Worker* findMostFreeWorker(Node *head)
{	
	Node* current = head;
	Worker* worker = (Worker*)current->data;
	current = current->next;

	while(current != NULL)
	{
		Worker *nextWorker = (Worker*)current->data;
		if (worker->dataCount > nextWorker->dataCount)
		{
			worker = nextWorker;
		}
		current = current->next;
		
	}

	return worker;
}

void sendDataToWorker(Worker* worker, Queue* clientMessages) {

   

    if (worker == NULL) {
        printf("Nevalidan Worker!\n");
        return;
    }

    char message[BUFFER_SIZE] = { 0 };
    {
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
        dequeue(clientMessages, message);
    }

    // Preparing the Message structure
    Message msg = { TEXT_MESSAGE, "" };
    strncpy_s(msg.content, message, sizeof(msg.content) - 1);
    msg.content[sizeof(msg.content) - 1] = '\0';  // Ensure null-termination

    addMessageToWorker(worker, &msg);

    // Instead of dynamic memory allocation, use the message buffer directly
    // Send the message to the worker
   

    int bytesSent = send(worker->socketFd, message, static_cast<int>(strlen(message)), 0);
    if (bytesSent == SOCKET_ERROR) {
        printf("Greška pri slanju poruke workeru.\n");
    }
    else {
        printf("Poruka poslata workeru sa ID: %d\n", worker->id);
    }
}


// Pomocna funkcija za slanje FREE_QUEUE komande pojedinačnom workeru
void sendFreeQueueCommandToWorker(Worker* worker) {
    const char* freeQueueCommand = "FREE_QUEUE";
    // Pretpostavljamo da Worker ima polje socket (worker->socket)
    send(worker->socketFd, freeQueueCommand, strlen(freeQueueCommand), 0);
    
}

void redistributeMessages(Queue* clientMessages, Node* workers) {

    // 3. Slanje FREE_QUEUE komande svim workerima
    {
        //std::lock_guard<std::mutex> lock(workersMutex);  // Zaključaj listu workera
        Node* current = workers;
        while (current != NULL) {
            Worker* worker = (Worker*)current->data;
            {
                // Pošalji pojedinačnom workeru poruku FREE_QUEUE
                sendFreeQueueCommandToWorker(worker);
            }
            current = current->next;
        }
    }


    // 1. SKLAPANJE: Prikupi sve neobrađene poruke sa svih workera i prebaci ih u clientMessages
    {
        //std::lock_guard<std::mutex> lock(workersMutex);  // Zaključaj listu workera
        Node* current = workers;
        while (current != NULL) {
            Worker* worker = (Worker*)current->data;
            {
                // Zaključaj rad s porukama unutar workera
                while (worker->dataCount > 0) {
                    Message* msgPtr = removeMessageFromWorker(worker);
                    if (msgPtr != NULL) {
                        {
                            std::lock_guard<std::mutex> queueLock(clientMessageQueueMutex);
                            enqueue(clientMessages, msgPtr->content);
                        }
                        free(msgPtr);  // Oslobodi memoriju poruke (alocirana u addMessageToWorker)
                    }
                }
            }
            current = current->next;
        }
    }

    // 2. Redistribucija poruka: Uzimanje poruka iz clientMessages i slanje najraspoloživijem worker-u
    while (true) {
        {
            // Provjeri da li je red prazan
            std::lock_guard<std::mutex> queueLock(clientMessageQueueMutex);
            if (isEmpty(clientMessages)) {
                break;
            }
        }

        Worker* bestWorker = nullptr;
        {
            //std::lock_guard<std::mutex> lock(workersMutex);
            bestWorker = findMostFreeWorker(workers);
        }

        if (bestWorker == nullptr) {
            std::cerr << "Nema dostupnih workera za redistribuciju.\n";
            break;
        }

        // Funkcija sendDataToWorker zaključava clientMessageQueueMutex, uzima poruku i šalje je workeru
        sendDataToWorker(bestWorker, clientMessages);
    }

}