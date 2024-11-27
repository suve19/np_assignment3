#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

// Structure to pass multiple arguments to the thread function
typedef struct {
    int sock;
    char client_name[13];  // Max 12 characters + null terminator
} thread_args;


void* receive_messages(void* args_ptr) {
    thread_args* args = (thread_args*)args_ptr;
    int sock = args->sock;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate the received string

            // Check for "MSG " prefix
            if (strncmp(buffer, "MSG ", 4) == 0) {
                char* nick_start = buffer + 4;   // Start after "MSG "
                char* message_start = strchr(nick_start, ' '); // Find the space after <nick>

                if (message_start) {
                    *message_start = '\0'; // Split <nick> and <message>
                    message_start++;       // Point to the start of <message>
                    printf("%s: %s\n", nick_start, message_start); // Format as "<nick>: <message>"
                } else {
                    printf("Invalid message format\n"); // Handle cases with no space after <nick>
                }
            } else {
                printf("%s\n", buffer); // Print other server messages as is
            }
            fflush(stdout);

            // Check if the message is "HELLO 1\n" and reply with "NICK <nickname>"
            if (strcmp(buffer, "HELLO 1\n") == 0) {
                char reply[300];
                snprintf(reply, sizeof(reply), "NICK %s\n", args->client_name);
                send(sock, reply, strlen(reply), 0);
            }
        } else if (bytes_received == 0) {
            printf("Server disconnected\n");
            fflush(stdout);
            break;
        } else {
            perror("Error receiving message from server");
            fflush(stderr);
            break;
        }
    }
    return NULL;
}

// Function to validate nickname
int validate_nickname(const char* nickname) {
    if (strlen(nickname) > 12) {
        fprintf(stderr, "Error: Nickname must not exceed 12 characters.\n");
        fflush(stderr);
        return 0;
    }

    regex_t regex;
    const char* pattern = "^[A-Za-z0-9_]+$";

    if (regcomp(&regex, pattern, REG_EXTENDED)) {
        fprintf(stderr, "Error compiling regex.\n");
        fflush(stderr);
        return 0;
    }

    int reti = regexec(&regex, nickname, 0, NULL, 0);
    regfree(&regex);

    if (reti) {
        fprintf(stderr, "Error: Nickname can only contain alphanumeric characters and underscores.\n");
        fflush(stderr);
        return 0;
    }

    return 1;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip>:<port> <name>\n", argv[0]);
        fflush(stderr);
        return -1;
    }

    // Parse <ip>:<port>
    char* arg = argv[1];
    char* colon_pos = strchr(arg, ':');
    if (!colon_pos) {
        fprintf(stderr, "Error: Invalid format. Use <ip>:<port>\n");
        fflush(stderr);
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
        fflush(stderr);
        return -1;
    }

    // Specify server address and port
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        fflush(stderr);
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR");
        fflush(stderr);
        return -1;
    }

    // Prepare arguments for the thread
    thread_args args;
    args.sock = sock;
    strncpy(args.client_name, client_name, sizeof(args.client_name) - 1);
    args.client_name[sizeof(args.client_name) - 1] = '\0';

    // Create a separate thread for receiving messages
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_messages, &args) != 0) {
        perror("Error creating thread");
        fflush(stderr);
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
            fflush(stderr);
            continue;
        }

        // Prepend "MSG " to the message
        char full_message[300];
        snprintf(full_message, sizeof(full_message), "MSG %s", message);
        send(sock, full_message, strlen(full_message), 0);
    }

    // Close the socket and wait for the receive thread to finish
    close(sock);
    pthread_join(receive_thread, NULL);

    return 0;
}
