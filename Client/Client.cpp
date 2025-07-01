#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
#include <condition_variable>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP_ADDRESS "127.0.0.1"
#define SERVER_PORT        5059
#define BUFFER_SIZE        256

int expectedAcks = 0;
int receivedAcks = 0;
std::mutex ackMutex;
std::condition_variable ackCV;

std::string generateRandomMessage(int messageNum) {
    return "Poruka broj " + std::to_string(messageNum);
}

void sendMessages(SOCKET clientSocket) {
    char data[BUFFER_SIZE];
    int option = 0;

    printf("Izaberite opciju:\n");
    printf("1. Unesite poruku za server (ili 'end' za prekid)\n");
    printf("2. Posaljite 2000 razlicitih poruka\n");
    printf("Opcija: ");
    scanf_s("%d", &option);
    getchar();  

    if (option == 1) {
        while (true) {
            printf("Unesite poruku za server (ili 'end' za prekid): ");
            gets_s(data, sizeof(data));

            if (strcmp(data, "end") == 0) {
                printf("Prekidanje komunikacije sa serverom.\n");
                break;
            }

            int len = (int)strlen(data);
            if (send(clientSocket, data, len, 0) == SOCKET_ERROR) {
                printf("Greska pri slanju podataka. Kod greške: %d\n", WSAGetLastError());
                break;
            }
            printf("Poruka poslata serveru!\n");
        }
    }
    else if (option == 2) {
        expectedAcks = 2000; //PROMENI ZA VECI TEST

        for (int i = 1; i <= expectedAcks; ++i) {
            std::string message = generateRandomMessage(i);
            message += "\n";
            std::this_thread::sleep_for(std::chrono::microseconds(75));

            int sz = (int)message.size();
            if (send(clientSocket, message.c_str(), sz, 0) == SOCKET_ERROR) {
                printf("Greška pri slanju podataka. Kod greške: %d\n", WSAGetLastError());
                break;
            }
            printf("Poruka %d poslata serveru!\n", i);
        }
    }
    else {
        printf("Nepoznata opcija!\n");
    }

  
}

void receiveMessages(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                printf("Server je zatvorio vezu.\n");
            }
            else {
                printf("Greska pri primanju podataka. Kod greske: %d\n", WSAGetLastError());
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        printf("Poruka od servera: %s\n", buffer);

        if (strstr(buffer, "LB primio msg_id=") != nullptr) {
            std::lock_guard<std::mutex> lock(ackMutex);
            receivedAcks++;
            printf("Broj primljenih potvrdnih poruka: %d\n", receivedAcks);
            if (receivedAcks >= expectedAcks) {
                ackCV.notify_one();
            }
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greska pri inicijalizaciji Winsock-a. Kod greske: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        printf("Greska pri kreiranju soketa. Kod greske: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP_ADDRESS, &serverAddress.sin_addr);
    serverAddress.sin_port = htons(SERVER_PORT);

    if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Neuspesno povezivanje na server. Kod greske: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    printf("Povezan na server!\n");


    std::thread sendThread(sendMessages, clientSocket);
    std::thread receiveThread(receiveMessages, clientSocket);
    sendThread.join();

    if (expectedAcks > 0) {
        std::unique_lock<std::mutex> lock(ackMutex);
        ackCV.wait(lock, [] { return receivedAcks >= expectedAcks; });
        printf("Primljene sve potvrde (%d/%d).\n", receivedAcks, expectedAcks);
    }

    shutdown(clientSocket, SD_SEND);
    receiveThread.join();
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
