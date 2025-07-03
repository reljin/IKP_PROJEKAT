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

    addMessageToWorker(worker, &localMsg);

    int len = static_cast<int>(strlen(localMsg.content));
    int sent = send(worker->socketFd,
        localMsg.content,
        len,
        0);

    if (sent == SOCKET_ERROR) {
        printf("Greska pri slanju workeru (ID=%d).\n", worker->id);
    }
    else {
        printf("Poslato %d bajta workeru (ID=%d): %s\n",
            sent, worker->id, localMsg.content);
    }
}



void sendFreeQueueCommandToWorker(Worker* worker) {
    const char* freeQueueCommand = "FREE_QUEUE"; 
    send(worker->socketFd, freeQueueCommand, strlen(freeQueueCommand), 0);
    
}

#include <atomic>
std::atomic<bool> isRedistributeActive(false);

//ako worker opadne a svi workeri su vec primili poruke i samo obradjuju

void redistributeMessagesDead(Queue* clientMessages, Node* workers) {

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    if (isRedistributeActive.load()) {
        printf("Glavna redistribucija je aktivna. Dead redistribucija se prekida.\n");
        return;
    }

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

void redistributeMessages(Queue* clientMessages, Node* workers) {

    isRedistributeActive.store(true);

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
    isRedistributeActive.store(false);

}