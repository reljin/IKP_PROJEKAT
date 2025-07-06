#include <cstdio>       
#include <winsock2.h>   
#include <ws2tcpip.h>   
#include <thread>      
#include <chrono>      
#include <string>      
#include <fstream>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "queue.h"   

#include <windows.h>

HANDLE g_fileMutexHandle = NULL;

#pragma comment(lib, "ws2_32.lib")  

#define SERVER_PORT_WORKER 6060  
#define BUFFER_SIZE 256          
#define REPLICATOR_PORT 6061


std::mutex receivedMessagesQueueMutex;
std::condition_variable receivedMessagesQueueCV;
Queue* receivedMessagesQueue = nullptr;
std::atomic<bool> stopWorker(false);
SOCKET replicatorSocket;
std::mutex fileMutex;


void saveData(const char* buffer) {
    if (g_fileMutexHandle) {
        DWORD waitResult = WaitForSingleObject(g_fileMutexHandle, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            std::ofstream file("workerOutput.txt", std::ios::app);
            if (!file) {
                printf("Greska pri otvaranju fajla.\n");
            }
            else {
                file << buffer << std::endl;
                file.close();
            }
            ReleaseMutex(g_fileMutexHandle);
        }
        else {
            printf("Nije uspeo WaitForSingleObject za mutex. Kod greske: %lu\n", GetLastError());
        }
    }
    else {
        // Fallback: ako nema mutex, samo piši bez sinhronizacije
        std::ofstream file("workerOutput.txt", std::ios::app);
        if (!file) {
            printf("Greska pri otvaranju fajla.\n");
            return;
        }
        file << buffer << std::endl;
        file.close();
    }
}



void processMessages(SOCKET workerSocket) {
    char* queueStoredBuffer = (char*)malloc(receivedMessagesQueue->dataSize);

    if (!queueStoredBuffer) {
        perror("Greska pri alokaciji memorije za queueStoredBuffer");
        return;
    }

    while (true) {
        std::unique_lock<std::mutex> lock(receivedMessagesQueueMutex);
        receivedMessagesQueueCV.wait(lock, [] { return !isEmpty(receivedMessagesQueue) || stopWorker.load(); });

        if (stopWorker.load()) {
            free(queueStoredBuffer);
            break;
        }

        memset(queueStoredBuffer, 0, receivedMessagesQueue->dataSize);

        // Dequeue raw data into our buffer
        dequeue(receivedMessagesQueue, queueStoredBuffer);

        // Properly null-terminate at actual end of string
        size_t msgLen = strnlen(queueStoredBuffer, receivedMessagesQueue->dataSize);
        queueStoredBuffer[msgLen] = '\0';     

        lock.unlock();         

        saveData(queueStoredBuffer);

        //std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Simulacija obrade ne radi iznad 30???
      
        std::string response = std::string(queueStoredBuffer) + "\n";
        send(workerSocket, response.c_str(), response.size(), 0);

        
        send(replicatorSocket, response.c_str(), response.size(), 0);
    }

    free(queueStoredBuffer);
}

void receiveData(SOCKET workerSocket) {
    char buffer[BUFFER_SIZE];
    std::string leftover;  // čuva fragment nedovršene poruke

    while (true) {
        int bytesReceived = recv(workerSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0)
            break;

        buffer[bytesReceived] = '\0';

        if (bytesReceived > 0 && std::string(buffer, bytesReceived).find('\n') == std::string::npos) {
            printf("\nRAW (bez \\n) (%d bajtova): ", bytesReceived);
            for (int i = 0; i < bytesReceived; ++i)
                printf("%02X ", (unsigned char)buffer[i]);
            printf("\n");
        }

        leftover += buffer;

        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string oneMessage = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);
            printf("Poruka: %s\n", oneMessage.c_str());

            {
                std::lock_guard<std::mutex> lock(receivedMessagesQueueMutex);


                if (oneMessage == "FREE_QUEUE") {
                    printf("FREE_QUEUE\n");
                    clearQueue(receivedMessagesQueue);
                    continue;
                }

                // Alociraj mutable bafer i kopiraj poruku
                char* msgBuf = (char*)malloc(BUFFER_SIZE);

                if (!msgBuf) {
                    fprintf(stderr, "Greška pri alokaciji memorije za poruku\n");
                    continue;
                }
                memset(msgBuf, 0, BUFFER_SIZE);
                strncpy_s(msgBuf, BUFFER_SIZE, oneMessage.c_str(), _TRUNCATE);

                enqueue(receivedMessagesQueue, msgBuf);
                free(msgBuf);
            }

            receivedMessagesQueueCV.notify_one();
        }
    }

    stopWorker.store(true);
    receivedMessagesQueueCV.notify_all();
}

void handleWorker(SOCKET workerSocket) {
    std::thread processingThread(processMessages, workerSocket);
    receiveData(workerSocket);

    processingThread.join();
    closesocket(workerSocket);
    closesocket(replicatorSocket); 

    {
        std::lock_guard<std::mutex> lock(receivedMessagesQueueMutex);
        freeQueue(receivedMessagesQueue);
    }
}
#include "message.h"

int main() {
    //std::ofstream("workerOutput.txt", std::ios::trunc).close();

    g_fileMutexHandle = CreateMutex(NULL, FALSE, L"Global\\WorkerFileMutex");
    if (g_fileMutexHandle == NULL) {
        printf("Ne mogu da kreiram named mutex. Greska: %lu\n", GetLastError());
        // Možeš odlučiti da li prekidaš ili nastavljaš
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greska pri inicijalizaciji Winsock-a.\n");
        return 1;
    }

    receivedMessagesQueue = createQueue(BUFFER_SIZE);
    if (!receivedMessagesQueue) {
        printf("Greska pri kreiranju reda klijenata.\n");
        WSACleanup();
        return 1;
    }

  
    SOCKET workerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (workerSocket == INVALID_SOCKET) {
        printf("Greška pri kreiranju soketa.\n");
        WSACleanup();
        return 1;
    }

 
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT_WORKER);

    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
        printf("Greska pri konverziji IP adrese.\n");
        closesocket(workerSocket);
        WSACleanup();
        return 1;
    }

    
    if (connect(workerSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Greska pri povezivanju sa Load Balancerom.\n");
        closesocket(workerSocket);
        WSACleanup();
        return 1;
    }

  
    replicatorSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (replicatorSocket == INVALID_SOCKET) {
        printf("Greka pri kreiranju socket-a za replikator.\n");
    } else {
        sockaddr_in replAddr;
        replAddr.sin_family = AF_INET;
        replAddr.sin_port = htons(REPLICATOR_PORT);
        inet_pton(AF_INET, "127.0.0.1", &replAddr.sin_addr);

        if (connect(replicatorSocket, (sockaddr*)&replAddr, sizeof(replAddr)) == SOCKET_ERROR) {
            printf("Neuspela konekcija na replikator.\n");
            closesocket(replicatorSocket);
        } else {
            printf("Povezan sa replikatorom.\n");
        }
    }

    printf("Worker povezan sa Load Balancerom na portu: %d\n", SERVER_PORT_WORKER);

   
    std::string message = "Podaci od Workera";
    send(workerSocket, message.c_str(), message.size(), 0);

   
    std::thread workerThread(handleWorker, workerSocket);
  

   
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5000));
    }

 
    closesocket(workerSocket);
    WSACleanup();

    if (g_fileMutexHandle) {
        CloseHandle(g_fileMutexHandle);
        g_fileMutexHandle = NULL;
    }

    return 0;
}