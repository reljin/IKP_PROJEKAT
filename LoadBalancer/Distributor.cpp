#include "Distributor.h"
#include "message.h"
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

    Message localMsg;
    {
        std::lock_guard<std::mutex> lock(clientMessageQueueMutex);
        dequeue(clientMessages, &localMsg);
    }

    char serialized[BUFFER_SIZE] = { 0 };
    snprintf(serialized, sizeof(serialized), "%d|%s", localMsg.msg_id, localMsg.content);

    // kopiraj poruku u workera (napravi kopiju)
    addMessageToWorker(worker, &localMsg);

  
    int bytesSent = send(worker->socketFd, serialized, static_cast<int>(strlen(serialized)), 0);
    if (bytesSent == SOCKET_ERROR) {
        printf("Greska pri slanju poruke workeru.\n");
    }
    else {
        printf("Poruka poslata workeru sa ID: %d\n", worker->id);
    }
}



void sendFreeQueueCommandToWorker(Worker* worker) {
    const char* freeQueueCommand = "FREE_QUEUE"; 
    send(worker->socketFd, freeQueueCommand, strlen(freeQueueCommand), 0);
    
}

void redistributeMessages(Queue* clientMessages, Node* workers) {

    //Slanje FREE_QUEUE komande svim workerima
    {

        Node* current = workers;
        while (current != NULL) {
            Worker* worker = (Worker*)current->data;
            {
                sendFreeQueueCommandToWorker(worker);
            }
            current = current->next;
        }
    }


    //Prikupi sve neobrađene poruke sa svih workera i prebaci ih u clientMessages
    {
       
        Node* current = workers;
        while (current != NULL) {
            Worker* worker = (Worker*)current->data;
            {
             
                while (worker->dataCount > 0) {
                    Message* msgPtr = removeMessageFromWorker(worker);
                    if (msgPtr != NULL) {
                        {
                            std::lock_guard<std::mutex> queueLock(clientMessageQueueMutex);
                            enqueue(clientMessages, msgPtr->content);
                        }
                        free(msgPtr); 
                    }
                }
            }
            current = current->next;
        }
    }

    //Uzimanje poruka iz clientMessages i slanje najraspolozivijem worker-u
    while (true) {
        {
      
            std::lock_guard<std::mutex> queueLock(clientMessageQueueMutex);
            if (isEmpty(clientMessages)) {
                break;
            }
        }

        Worker* bestWorker = nullptr;
        {
       
            bestWorker = findMostFreeWorker(workers);
        }

        if (bestWorker == nullptr) {
            std::cerr << "Nema dostupnih workera za redistribuciju.\n";
            break;
        }

        sendDataToWorker(bestWorker, clientMessages);
    }

}