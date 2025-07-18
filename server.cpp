#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <mutex>
#include <string>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

std::vector<SOCKET> clients;
std::mutex clients_mutex;
std::mutex console_mutex;

class ChatServer {
private:
    SOCKET serverSocket;
    int port;

public:
    ChatServer(int serverPort) : port(serverPort), serverSocket(INVALID_SOCKET) {}

    bool initialize() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printToConsole("WSAStartup failed");
            return false;
        }

        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET) {
            printToConsole("Socket creation failed");
            WSACleanup();
            return false;
        }

        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            printToConsole("Bind failed");
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }

        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            printToConsole("Listen failed");
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }

        printToConsole("WSAStartup successful");
        printToConsole("Socket creation successful");
        printToConsole("Bind successful");
        printToConsole("Listen successful");
        printToConsole("Server started on all available interfaces:" + std::to_string(port));
        printToConsole("Find your IP with 'ipconfig' and share it with clients");
        printToConsole("Waiting for clients...");

        return true;
    }

    void start() {
        while (true) {
            sockaddr_in clientAddr;
            int clientSize = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);

            if (clientSocket != INVALID_SOCKET) {
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
                
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    clients.push_back(clientSocket);
                }

                printToConsole(std::string(clientIP) + " connected");
                
                std::thread clientThread(&ChatServer::handleClient, this, clientSocket, std::string(clientIP));
                clientThread.detach();
            }
        }
    }

    void handleClient(SOCKET clientSocket, std::string clientIP) {
        char buffer[1024];
        std::string clientName = "Unknown";
        bool nameSet = false;

        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesReceived <= 0) {
                break;
            }

            buffer[bytesReceived] = '\0';
            std::string message(buffer);
            
            message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
            message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());

            if (message.empty()) continue;

            if (!nameSet) {
                size_t colonPos = message.find(':');
                if (colonPos != std::string::npos) {
                clientName = message.substr(0, colonPos);
                message = message.substr(colonPos + 1);
                if (!message.empty() && message[0] == ' ') {
                    message = message.substr(1);
                }
                nameSet = true;
                printToConsole(clientName + " joined the chat"); 
            } else {
                        clientName = message;
                        nameSet = true;
                        printToConsole(clientName + " joined the chat");
            continue;
            }
        } else {
    size_t colonPos = message.find(':');
    if (colonPos != std::string::npos) {
        message = message.substr(colonPos + 1);
        if (!message.empty() && message[0] == ' ') {
            message = message.substr(1);
        }
    }
}

if (!message.empty()) {
    std::string fullMessage = clientName + ": " + message;
    printToConsole(fullMessage);
    broadcastMessage(fullMessage, clientSocket);
}
        }

        removeClient(clientSocket);
        printToConsole(clientName + " (" + clientIP + ") disconnected");
        closesocket(clientSocket);
    }

    void broadcastMessage(const std::string& message, SOCKET senderSocket) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        
        std::string messageWithNewline = message + "\n";
        
        for (auto it = clients.begin(); it != clients.end();) {
            if (*it != senderSocket) {
                int result = send(*it, messageWithNewline.c_str(), messageWithNewline.length(), 0);
                if (result == SOCKET_ERROR) {
                    closesocket(*it);
                    it = clients.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    void removeClient(SOCKET clientSocket) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
    }

    void printToConsole(const std::string& message) {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << message << std::endl;
        std::cout.flush();
    }

    ~ChatServer() {
        if (serverSocket != INVALID_SOCKET) {
            closesocket(serverSocket);
        }
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            for (SOCKET client : clients) {
                closesocket(client);
            }
            clients.clear();
        }
        
        WSACleanup();
    }
};

int main() {
    ChatServer server(8000);
    
    if (!server.initialize()) {
        std::cout << "Failed to initialize server" << std::endl;
        return 1;
    }
    
    server.start();
    return 0;
}