#include <iostream>
#include <cstring>
#include <regex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>

// Function to handle receiving messages from the server
void receive_messages(int sock) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            std::cout << std::string(buffer, bytes_received) << std::endl;
        } else if (bytes_received == 0) {
            std::cout << "Server disconnected\n";
            break;
        } else {
            std::cerr << "Error receiving message from server\n";
            break;
        }
    }
}

bool validate_nickname(const std::string& nickname) {
    if (nickname.length() > 12) {
        std::cerr << "Error: Nickname must not exceed 12 characters.\n";
        return false;
    }

    std::regex nickname_pattern("^[A-Za-z0-9_]+$");
    if (!std::regex_match(nickname, nickname_pattern)) {
        std::cerr << "Error: Nickname can only contain alphanumeric characters and underscores.\n";
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip>:<port> <name>\n";
        return -1;
    }

    // Parse <ip>:<port>
    std::string arg = argv[1];
    size_t colon_pos = arg.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Error: Invalid format. Use <ip>:<port>\n";
        return -1;
    }
    std::string ip = arg.substr(0, colon_pos);
    int port = std::stoi(arg.substr(colon_pos + 1));

    // Parse nickname and validate
    std::string client_name = argv[2];
    if (!validate_nickname(client_name)) {
        return -1;
    }

    // Create socket
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error\n";
        return -1;
    }

    // Specify server address and port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported\n";
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed\n";
        return -1;
    }

    // Create a separate thread for receiving messages
    std::thread receive_thread(receive_messages, sock);

    // Main thread for sending messages
    while (true) {
        std::string message;
        std::getline(std::cin, message);

        // Exit on "exit" command
        if (message == "exit") {
            break;
        }

        // Validate message length
        if (message.length() > 255) {
            std::cerr << "Error: Message length must not exceed 255 characters.\n";
            continue;
        }

        // Prepend client name to the message
        std::string full_message = client_name + ": " + message;
        send(sock, full_message.c_str(), full_message.size(), 0);
    }

    // Close the socket and wait for the receive thread to finish
    close(sock);
    receive_thread.join();

    return 0;
}
