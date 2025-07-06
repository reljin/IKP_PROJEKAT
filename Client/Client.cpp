#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mutex>
#include <random>
#include <atomic>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP_ADDRESS "127.0.0.1"
#define SERVER_PORT        5059
#define BUFFER_SIZE        256

int expectedAcks = 0;
std::atomic<int> receivedAcks(0);
int selectedOption = 0;
std::mutex fileMutex;

std::string generateRandomMessage(int messageNum) {
    size_t len = 20;
    const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string s;
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, 51);
    while (len--) s += chars[dist(rng)];
    return s;
}

void saveData(const char* buffer) {
    std::lock_guard<std::mutex> lock(fileMutex);

    std::ofstream file("clientOutput.txt", std::ios::app);
    if (!file) {
        printf("Greska pri otvaranju fajla.\n");
        return;
    }

    file << buffer << std::endl;
    file.close();
}

void sendMessages(SOCKET clientSocket) {
    char data[BUFFER_SIZE];

    if (selectedOption == 1) {
        while (true) {
            printf("Unesite poruku za server (ili 'end' za prekid): ");
            gets_s(data, sizeof(data));

            if (strcmp(data, "end") == 0) {
                printf("Prekidanje komunikacije sa serverom.\n");
                break;
            }

            strcat_s(data, sizeof(data), "\n"); // dodaj newline

            int len = (int)strlen(data);
            if (send(clientSocket, data, len, 0) == SOCKET_ERROR) {
                printf("Greska pri slanju podataka. Kod greške: %d\n", WSAGetLastError());
                break;
            }
            printf("Poruka poslata serveru!\n");
        }
    }
    else if (selectedOption == 2) {
        expectedAcks = 2000;

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
    if (selectedOption == 2) {
        DWORD timeout = 0; // 10 sekundi
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    }
    else {
        DWORD timeout = 0; // bez timeouta
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    }

    char buffer[BUFFER_SIZE];
    std::string leftover;

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            int err = WSAGetLastError();
            if (bytesReceived == 0) {
                printf("Server je zatvorio vezu.\n");
            }
            else if (selectedOption == 2 && err == WSAETIMEDOUT) {
                printf("Nije primljena poruka u roku od 10 sekundi. Zatvaram konekciju.\n");
            }
            else {
                printf("Greska pri primanju podataka. Kod greske: %d\n", err);
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        leftover.append(buffer, bytesReceived);

        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string oneMessage = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);

            if (!oneMessage.empty() && oneMessage.back() == '\r') {
                oneMessage.pop_back();
            }

            printf("Poruka od servera: %s\n", oneMessage.c_str());
            saveData(oneMessage.c_str());

            if (selectedOption == 2) {
                receivedAcks++;
                if (receivedAcks % 100 == 0 || receivedAcks == expectedAcks) {
                    printf("Primljene potvrde: %d / %d\n", receivedAcks.load(), expectedAcks);
                }

                if (receivedAcks >= expectedAcks) {
                    printf("Primljene sve potvrde. Zavrsavam receive nit.\n");
                    return;
                }
            }
        }
    }
}

int main() {
    std::ofstream("clientOutput.txt", std::ios::trunc).close();

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

    // Izbor opcije pre pokretanja niti
    printf("Izaberite opciju:\n");
    printf("1. Unesite poruku za server (ili 'end' za prekid)\n");
    printf("2. Posaljite 2000 razlicitih poruka\n");
    printf("Opcija: ");
    scanf_s("%d", &selectedOption);
    

    std::thread sender(sendMessages, clientSocket);
    std::thread receiver(receiveMessages, clientSocket);

    sender.join();
    receiver.join();

    shutdown(clientSocket, SD_SEND);
    closesocket(clientSocket);
    WSACleanup();

    scanf_s("%d", &selectedOption);
    getchar();

    return 0;
}
