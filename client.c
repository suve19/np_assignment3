#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

// Function to handle receiving messages from the server
void* receive_messages(void* sock_ptr) {
    int sock = *(int*)sock_ptr;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0) {
            printf("%s\n", buffer);
        } else if (bytes_received == 0) {
            printf("Server disconnected\n");
            break;
        } else {
            perror("Error receiving message from server");
            break;
        }
    }
    return NULL;
}

// Function to validate nickname
int validate_nickname(const char* nickname) {
    if (strlen(nickname) > 12) {
        fprintf(stderr, "Error: Nickname must not exceed 12 characters.\n");
        return 0;
    }

    regex_t regex;
    const char* pattern = "^[A-Za-z0-9_]+$";

    if (regcomp(&regex, pattern, REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex.\n");
        return 0;
    }

    int reti = regexec(&regex, nickname, 0, NULL, 0);
    regfree(&regex);

    if (reti) {
        fprintf(stderr, "Error: Nickname can only contain alphanumeric characters and underscores.\n");
        return 0;
    }

    return 1;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip>:<port> <name>\n", argv[0]);
        return -1;
    }

    // Parse <ip>:<port>
    char* arg = argv[1];
    char* colon_pos = strchr(arg, ':');
    if (!colon_pos) {
        fprintf(stderr, "Error: Invalid format. Use <ip>:<port>\n");
        return -1;
    }

    char ip[100];
    int port;
    strncpy(ip, arg, colon_pos - arg);
    ip[colon_pos - arg] = '\0';
    port = atoi(colon_pos + 1);

    // Validate nickname
    char* client_name = argv[2];
    if (!validate_nickname(client_name)) {
        return -1;
    }

    // Create socket
    int sock;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    // Specify server address and port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR");
        return -1;
    }

    // Create a separate thread for receiving messages
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_messages, &sock) != 0) {
        perror("Error creating thread");
        return -1;
    }

    // Main thread for sending messages
    char message[256];
    while (1) {
        if (fgets(message, sizeof(message), stdin) == NULL) {
            break;
        }
        message[strcspn(message, "\n")] = '\0';  // Remove newline character

        // Exit on "exit" command
        if (strcmp(message, "exit") == 0) {
            break;
        }

        // Validate message length
        if (strlen(message) > 255) {
            fprintf(stderr, "Error: Message length must not exceed 255 characters.\n");
            continue;
        }

        // Prepend client name to the message
        char full_message[300];
        snprintf(full_message, sizeof(full_message), "%s: %s", client_name, message);
        send(sock, full_message, strlen(full_message), 0);
    }

    // Close the socket and wait for the receive thread to finish
    close(sock);
    pthread_join(receive_thread, NULL);

    return 0;
}
