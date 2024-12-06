#include <iostream>
#include <thread>
#include <winsock2.h>
#include <vector>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 5059
#define BUFFER_SIZE 256

std::mutex clientSocketsMutex;
std::vector<SOCKET> clientSockets;

void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                std::cout << "Klijent se diskonektovao." << std::endl;
            }
            else {
                std::cerr << "Greška pri primanju podataka. Kod greške: " << WSAGetLastError() << std::endl;
            }
            break;
        }

       
        buffer[bytesReceived] = '\0';

        
        std::cout << "Poruka od klijenta: " << buffer << std::endl;

       
        std::string response = "Poruka primljena: " + std::string(buffer);
        send(clientSocket, response.c_str(), response.size(), 0);
    }

    // Close the client socket and remove from clientSockets vector
    closesocket(clientSocket);

    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        auto it = std::find(clientSockets.begin(), clientSockets.end(), clientSocket);
        if (it != clientSockets.end()) {
            clientSockets.erase(it);
        }
    }

    std::cout << "Završena nit za klijenta." << std::endl;
}

void clientListener(SOCKET listenSocket) {
    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Greška pri prihvatanju veze." << std::endl;
            continue;
        }

        std::cout << "Klijent povezan!" << std::endl;

        //punim vektor soketa ovde, ne sme vise klijentskih niti da mi menja ovo istovremeno

        {
            std::lock_guard<std::mutex> lock(clientSocketsMutex);
            clientSockets.push_back(clientSocket);
        }

        // Novi thread za rukovanje klijentom
        std::thread(handleClient, clientSocket).detach();
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Greška pri inicijalizaciji Winsock-a." << std::endl;
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Greška pri kreiranju soketa." << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (bind(listenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Greška pri bindenju soketa." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Greška pri postavljanju slušanja na soketu." << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Load Balancer osluškuje na portu: " << SERVER_PORT << std::endl;

    // Thread za prihvatanje konekcija od klijenata
    std::thread(clientListener, listenSocket).detach();

    // Load Balancer radi druge poslove OSTAVI OVAJ WHILE

    while (true) {
        std::cout << "Load Balancer radi druge poslove..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }


    closesocket(listenSocket);
    WSACleanup();
    return 0;
}