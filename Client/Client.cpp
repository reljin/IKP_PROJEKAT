#include <iostream>
#include <string>
#include <winsock2.h> // Windows socket API
#include <ws2tcpip.h> // Za inet_pton()

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP_ADDRESS "127.0.0.1"
#define SERVER_PORT 5059
#define BUFFER_SIZE 256

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Greska pri inicijalizaciji Winsock-a. Kod greske: " << WSAGetLastError() << std::endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Greska pri kreiranju soketa. Kod greske: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET; // IPv4 protokol
    inet_pton(AF_INET, SERVER_IP_ADDRESS, &serverAddress.sin_addr); // Moderni način za konverziju IP adrese
    serverAddress.sin_port = htons(SERVER_PORT);

    if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Neuspesno povezivanje na server. Kod greske: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::string data;
    std::cout << "Unesite poruku za server: ";
    std::getline(std::cin, data);

    if (send(clientSocket, data.c_str(), data.size(), 0) == SOCKET_ERROR) {
        std::cerr << "Greska pri slanju podataka. Kod greske: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Podaci poslati serveru!" << std::endl;

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
