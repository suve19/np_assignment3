#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <algorithm>

// Dictionary to store message queues for each client
std::unordered_map<int, std::queue<std::string>> message_buffers;

// Function to broadcast messages to all other clients
void broadcast_message(int sender_socket, const std::string& message, const std::vector<int>& clients) {
    for (int client : clients) {
        if (client != sender_socket) {
            message_buffers[client].push(message);  // Store message in each client's buffer
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./server <ip>:<port>\n";
        return -1;
    }

    // Parse IP and port from the argument
    std::string arg = argv[1];
    size_t colon_pos = arg.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid format. Use <ip>:<port>\n";
        return -1;
    }

    std::string ip = arg.substr(0, colon_pos);
    int port = std::stoi(arg.substr(colon_pos + 1));

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        std::cerr << "Socket creation failed\n";
        return -1;
    }

    // Define server address
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    address.sin_port = htons(port);

    // Bind socket to server address
    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed\n";
        close(server_socket);
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 3) < 0) {
        std::cerr << "Listen failed\n";
        close(server_socket);
        return -1;
    }

    std::cout << "Server is listening on " << ip << ":" << port << "...\n";

    fd_set read_fds, write_fds;
    std::vector<int> clients;  // List of client sockets
    int max_sd = server_socket;

    while (true) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(server_socket, &read_fds);

        for (int client : clients) {
            FD_SET(client, &read_fds);  // Add clients to read set
            if (!message_buffers[client].empty()) {
                FD_SET(client, &write_fds);  // Add to write set if there's a message to send
            }
            if (client > max_sd) max_sd = client;
        }

        // Use select() to monitor sockets
        int activity = select(max_sd + 1, &read_fds, &write_fds, nullptr, nullptr);
        if (activity < 0) {
            std::cerr << "Select error\n";
            break;
        }

        // Handle new connections
        if (FD_ISSET(server_socket, &read_fds)) {
            int client_socket = accept(server_socket, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            if (client_socket < 0) {
                std::cerr << "Accept failed\n";
                continue;
            }
            clients.push_back(client_socket);
            message_buffers[client_socket];  // Initialize message buffer for the client

            // Get client details
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            getpeername(client_socket, (struct sockaddr*)&client_addr, &client_len);
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);

            std::cout << "New client connected from " << client_ip << ":" << client_port << "\n";

            // Dummy protocol check (can be extended)
            std::string protocol = "allowed"; // Assume protocol is allowed for now
            if (protocol != "allowed") {
                std::cerr << "Client protocol not allowed, disconnecting\n";
                close(client_socket);
                clients.pop_back();
                message_buffers.erase(client_socket);
            }
        }

        // Handle messages from clients
        for (int client : clients) {
            if (FD_ISSET(client, &read_fds)) {
                char buffer[1024] = {0};
                int bytes_received = recv(client, buffer, sizeof(buffer), 0);
                if (bytes_received <= 0) {
                    // Client disconnected
                    std::cerr << "Client disconnected\n";
                    close(client);
                    clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
                    message_buffers.erase(client);
                    continue;
                }

                std::string message(buffer, bytes_received);
                std::cout << "Received message from client " << client << ": " << message << "\n";
                
                // Immediately broadcast the message to other clients
                broadcast_message(client, message, clients);
            }

            // Send buffered messages to clients
            if (FD_ISSET(client, &write_fds)) {
                while (!message_buffers[client].empty()) {
                    std::string message_to_send = message_buffers[client].front();
                    send(client, message_to_send.c_str(), message_to_send.length(), 0);
                    message_buffers[client].pop();
                }
            }
        }
    }

    close(server_socket);
    return 0;
}
