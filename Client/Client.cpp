// Client.cpp
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


// Generiše tekst "Poruka broj X"
std::string generateRandomMessage(int messageNum) {
    return "Poruka broj " + std::to_string(messageNum);
}

void sendMessages(SOCKET clientSocket) {
    char data[BUFFER_SIZE];
    int option = 0;

    printf("Izaberite opciju:\n");
    printf("1. Unesite poruku za server (ili 'end' za prekid)\n");
    printf("2. Posaljite 3000 različitih poruka\n");
    printf("Opcija: ");
    scanf_s("%d", &option);
    getchar();  // uklanja '\n' iz stdin

    if (option == 1) {
        // Opcija 1: Ručno slanje
        while (true) {
            printf("Unesite poruku za server (ili 'end' za prekid): ");
            gets_s(data, sizeof(data));

            if (strcmp(data, "end") == 0) {
                printf("Prekidanje komunikacije sa serverom.\n");
                break;
            }

            int len = (int)strlen(data);
            // Dodajemo '\n' na kraj radi lakšeg parsiranja (opciono)
            // Ako ne želite novi red, komentarišite sledeće dve linije:
            // if (len < BUFFER_SIZE - 1) { data[len] = '\n'; data[len+1] = '\0'; len++; }

            if (send(clientSocket, data, len, 0) == SOCKET_ERROR) {
                printf("Greška pri slanju podataka. Kod greške: %d\n", WSAGetLastError());
                break;
            }
            printf("Poruka poslata serveru!\n");
        }
    }
    else if (option == 2) {
        expectedAcks = 2000; // broj poruka koje očekujemo kao potvrđene

        for (int i = 1; i <= expectedAcks; ++i) {
            std::string message = generateRandomMessage(i);
            message += "\n";
            std::this_thread::sleep_for(std::chrono::microseconds(2));

            int sz = (int)message.size();
            if (send(clientSocket, message.c_str(), sz, 0) == SOCKET_ERROR) {
                printf("Greška pri slanju podataka. Kod greške: %d\n", WSAGetLastError());
                break;
            }
            printf("Poruka %d poslata serveru!\n", i);
        }

        // Ne radimo shutdown ovde - čekamo da sve ACK poruke stignu
    }
    else {
        printf("Nepoznata opcija!\n");
    }

    // Obavestimo server da više ne šaljemo
    shutdown(clientSocket, SD_SEND);
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
                printf("Greška pri primanju podataka. Kod greške: %d\n", WSAGetLastError());
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        printf("Poruka od servera: %s\n", buffer);

        // Ako je ACK od servera
        if (strstr(buffer, "LB primio msg_id=") != nullptr) {
            std::lock_guard<std::mutex> lock(ackMutex);
            receivedAcks++;
            printf("Broj primljenih poruka: %d\n",receivedAcks);
            if (receivedAcks >= expectedAcks) {
                ackCV.notify_one(); // signaliziramo main niti
            }
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greška pri inicijalizaciji Winsock-a. Kod greške: %d\n", WSAGetLastError());
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        printf("Greška pri kreiranju soketa. Kod greške: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP_ADDRESS, &serverAddress.sin_addr);
    serverAddress.sin_port = htons(SERVER_PORT);

    if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Neuspešno povezivanje na server. Kod greške: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    printf("Povezan na server!\n");

    // Pokrećemo dve niti: jedna šalje, druga prima
    std::thread sendThread(sendMessages, clientSocket);
    std::thread receiveThread(receiveMessages, clientSocket);
    sendThread.join();

    // Ako smo slali 3000 poruka, čekamo da primimo 3000 potvrda
    if (expectedAcks > 0) {
        std::unique_lock<std::mutex> lock(ackMutex);
        ackCV.wait(lock, [] { return receivedAcks >= expectedAcks; });
        printf("Primljene sve potvrde (%d/%d).\n", receivedAcks, expectedAcks);

        // Nakon toga možemo ugasiti socket za slanje
        shutdown(clientSocket, SD_SEND);
    }

    receiveThread.join();

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
