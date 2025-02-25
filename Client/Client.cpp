#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 5059
#define BUFFER_SIZE 256

std::string generateRandomMessage(int messageNum) {
    return "Poruka broj " + std::to_string(messageNum);
}

void sendMessages(SOCKET clientSocket) {
    std::string data;
    int option;

    // Prikazivanje opcija korisniku
    std::cout << "Izaberite opciju:" << std::endl;
    std::cout << "1. Unesite poruku za server (ili 'end' za prekid)" << std::endl;
    std::cout << "2. Pošaljite 100 različitih poruka" << std::endl;
    std::cin >> option;
    std::cin.ignore();  // Da bismo očistili buffer nakon unosa opcije

    if (option == 1) {
        // Opcija 1: Ručno slanje poruka
        while (true) {
            std::cout << "Unesite poruku za server (ili 'end' za prekid): ";
            std::getline(std::cin, data);

            if (data == "end") {
                std::cout << "Prekidanje komunikacije sa serverom." << std::endl;
                break;
            }
            
            if (send(clientSocket, data.c_str(), data.size(), 0) == SOCKET_ERROR) {
                std::cerr << "Greška pri slanju podataka. Kod greške: " << WSAGetLastError() << std::endl;
                break;
            }

            std::cout << "Poruka poslata serveru!" << std::endl;
        }
    }
    else if (option == 2) {
        // Opcija 2: Slanje 1000 različitih poruka
        for (int i = 1; i <= 5000; ++i) {
            data = generateRandomMessage(i);
            std::this_thread::sleep_for(std::chrono::microseconds(2)); // ispod 2 send konkatenira poruke
            if (send(clientSocket, data.c_str(), data.size(), 0) == SOCKET_ERROR) {
                std::cerr << "Greška pri slanju podataka. Kod greške: " << WSAGetLastError() << std::endl;
                break;
            }

            std::cout << "Poruka " << i << " poslata serveru!" << std::endl;
        }
    }
    else {
        std::cout << "Nepoznata opcija!" << std::endl;
    }

    // Zatvaranje soketa nakon prekida slanja
    shutdown(clientSocket, SD_SEND);
}

void receiveMessages(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                std::cout << "Server je zatvorio vezu." << std::endl;
            }
            else {
                std::cerr << "Greška pri primanju podataka. Kod greške: " << WSAGetLastError() << std::endl;
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        //std::cout << "\nPoruka od servera: " << buffer << std::endl;
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Greška pri inicijalizaciji Winsock-a. Kod greške: " << WSAGetLastError() << std::endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Greška pri kreiranju soketa. Kod greške: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP_ADDRESS, &serverAddress.sin_addr);
    serverAddress.sin_port = htons(SERVER_PORT);

    if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Neuspešno povezivanje na server. Kod greške: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Povezan na server!" << std::endl;

    // Kreiranje threadova za slanje i primanje poruka
    std::thread sendThread(sendMessages, clientSocket);
    std::thread receiveThread(receiveMessages, clientSocket);

    // Čekanje da se niti završe
    sendThread.join();
    receiveThread.join();

    // Zatvaranje konekcije i čišćenje Winsock-a
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
