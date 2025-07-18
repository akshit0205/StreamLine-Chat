#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <string>
#include <mutex>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

class ChatClient {
private:
    SOCKET clientSocket;
    std::string serverIP;
    int serverPort;
    std::string userName;
    bool connected;
    std::mutex console_mutex;

public:
    ChatClient() : clientSocket(INVALID_SOCKET), serverPort(8000), connected(false) {}

    bool initialize() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cout << "WSAStartup failed" << std::endl;
            return false;
        }

        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            std::cout << "Socket creation failed" << std::endl;
            WSACleanup();
            return false;
        }

        std::cout << "WSAStartup successful" << std::endl;
        std::cout << "Socket created successfully" << std::endl;
        return true;
    }

    bool connectToServer() {
        std::cout << "Enter server IP (or press Enter for localhost): ";
        std::getline(std::cin, serverIP);
        
        if (serverIP.empty()) {
            serverIP = "127.0.0.1";
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(serverPort);
        
        if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
            std::cout << "Invalid server IP address" << std::endl;
            return false;
        }

        std::cout << "Connecting to " << serverIP << ":" << serverPort << "..." << std::endl;

        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cout << "Connection failed" << std::endl;
            return false;
        }

        std::cout << "Connected successfully" << std::endl;
        connected = true;
        return true;
    }

    void getUserName() {
        std::cout << "Enter your name: ";
        std::getline(std::cin, userName);
        
        if (userName.empty()) {
            userName = "Anonymous";
        }
    }

    void startChat() {
        std::cout << "\nConnected to chat! Type your messages:" << std::endl;
        std::cout << "=====================================\n" << std::endl;

        std::thread receiveThread(&ChatClient::receiveMessages, this);
        receiveThread.detach();

        sendMessages();
    }

    void receiveMessages() {
        char buffer[1024];
        while (connected) {
            memset(buffer, 0, sizeof(buffer));
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesReceived <= 0) {
                {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "\nDisconnected from server" << std::endl;
                }
                connected = false;
                break;
            }

            buffer[bytesReceived] = '\0';
            std::string message(buffer);
            
            if (!message.empty() && message.back() == '\n') {
                message.pop_back();
            }
            if (!message.empty() && message.back() == '\r') {
                message.pop_back();
            }

            if (!message.empty()) {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "\r" << message << std::endl;
                std::cout << userName << ": ";
                std::cout.flush();
            }
        }
    }

    void sendMessages() {
        std::string message;
        
        while (connected) {
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << userName << ": ";
                std::cout.flush();
            }
            
            std::getline(std::cin, message);
            
            if (!connected) break;
            
            if (!message.empty()) {
                std::string fullMessage = userName + ": " + message;
                int result = send(clientSocket, fullMessage.c_str(), fullMessage.length(), 0);
                
                if (result == SOCKET_ERROR) {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "Failed to send message" << std::endl;
                    connected = false;
                    break;
                }
            }
        }
    }

    ~ChatClient() {
        connected = false;
        if (clientSocket != INVALID_SOCKET) {
            closesocket(clientSocket);
        }
        WSACleanup();
    }
};

int main() {
    ChatClient client;
    
    if (!client.initialize()) {
        std::cout << "Failed to initialize client" << std::endl;
        return 1;
    }
    
    if (!client.connectToServer()) {
        std::cout << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    client.getUserName();
    client.startChat();
    
    std::cout << "Press any key to exit..." << std::endl;
    _getch();
    return 0;
}