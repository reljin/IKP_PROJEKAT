#include <cstdio>       // Umesto <iostream> za printf i scanf
#include <winsock2.h>   // Winsock za mrežnu komunikaciju
#include <ws2tcpip.h>   // Dodatne funkcije za mrežnu komunikaciju (inet_pton)
#include <thread>       // C++ niti
#include <chrono>       // C++ vreme
#include <string>       // C++ string

#pragma comment(lib, "ws2_32.lib")  // Linkovanje Winsock biblioteke

#define SERVER_PORT_WORKER 6060  // Port na kojem worker osluškuje
#define BUFFER_SIZE 256          // Veličina bafera za komunikaciju

// Funkcija za obradu zahteva od Load Balancera
void handleWorker(SOCKET workerSocket) {
    char buffer[BUFFER_SIZE];

    while (true) {
        // Primanje podataka od Load Balancera
        int bytesReceived = recv(workerSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                printf("Load Balancer se diskonektovao.\n");
            }
            else {
                printf("Greska pri primanju podataka. Kod greske: %d\n", WSAGetLastError());
            }
            break;
        }

        buffer[bytesReceived] = '\0';  // Dodajemo null-terminator
        printf("Primljeni podaci od Load Balancera: %s\n", buffer);

        // Simulacija obrade podataka (1 sekunda)
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Slanje odgovora Load Balanceru
        std::string response = "Podaci obradjeni: " + std::string(buffer);
        send(workerSocket, response.c_str(), response.size(), 0);
    }

    closesocket(workerSocket);  // Zatvaranje soketa
    printf("Nit Workera zavrsena.\n");
}

int main() {
    // Inicijalizacija Winsock-a
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Greska pri inicijalizaciji Winsock-a.\n");
        return 1;
    }

    // Kreiranje soketa
    SOCKET workerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (workerSocket == INVALID_SOCKET) {
        printf("Greska pri kreiranju soketa.\n");
        WSACleanup();
        return 1;
    }

    // Podešavanje adrese servera (Load Balancer)
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT_WORKER);

    // Konverzija IP adrese iz stringa u binarnu formu
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
        printf("Greska pri konverziji IP adrese.\n");
        closesocket(workerSocket);
        WSACleanup();
        return 1;
    }

    // Povezivanje sa Load Balancerom
    if (connect(workerSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Greska pri povezivanju sa Load Balancerom.\n");
        closesocket(workerSocket);
        WSACleanup();
        return 1;
    }

    printf("Worker povezan sa Load Balancerom na portu: %d\n", SERVER_PORT_WORKER);

    // Slanje početne poruke Load Balanceru
    std::string message = "Podaci od Workera";
    send(workerSocket, message.c_str(), message.size(), 0);

    // Pokretanje niti za obradu zahteva
    std::thread workerThread(handleWorker, workerSocket);
    workerThread.detach();  // Odvajamo nit od glavnog toka

    // Glavni tok rada (simulacija drugih poslova)
    while (true) {
        printf("Worker radi druge poslove...\n");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // Zatvaranje soketa i čišćenje Winsock-a
    closesocket(workerSocket);
    WSACleanup();
    return 0;
}