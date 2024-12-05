#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <regex.h>

#define BUFFER_SIZE 1024

typedef struct {
    int sock;
    char client_name[50];
} thread_args;

void* receive_messages(void* arg) {
    int sock = *(int*)arg;
    char buffer[BUFFER_SIZE];
    char* message_list[BUFFER_SIZE]; // List to store messages
    int message_count = 0;          // Number of messages in the list

    while (1) {
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate the received data

            // Split buffer into messages using '\n'
            char* message = strtok(buffer, "\n");
            while (message != NULL) {
                // Add each complete message to the list
                message_list[message_count] = strdup(message);
                message_count++;
                message = strtok(NULL, "\n");
            }

            // Print all messages in the list
            for (int i = 0; i < message_count; i++) {
                if (strncmp(message_list[i], "MSG ", 4) == 0) {
                    char* nick_start = message_list[i] + 4; // Start after "MSG "
                    char* message_start = strchr(nick_start, ' '); // Find the space after <nick>

                    if (message_start) {
                        *message_start = '\0'; // Split <nick> and <message>
                        message_start++;       // Point to the start of <message>
                        printf("%s: %s\n", nick_start, message_start); // Print "<nick>: <message>"
                    } else {
                        printf("Invalid message format: %s\n", message_list[i]);
                    }
                } else {
                    printf("%s\n", message_list[i]); // Print other messages as is
                }

                free(message_list[i]); // Free the allocated memory
            }

            // Clear the list after printing
            message_count = 0;

        } else if (bytes_received == 0) {
            printf("Server disconnected\n");
            close(sock);
            pthread_exit(NULL);
        } else {
            perror("Error receiving message");
            close(sock);
            pthread_exit(NULL);
        }
    }
}

// Function to perform the handshake
void perform_handshake(int sock, const char* client_name) {
    char buffer[BUFFER_SIZE];

    // Receive "HELLO 1\n"
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received string

        if (strcmp(buffer, "HELLO 1\n") == 0) {
            char reply[300];
            snprintf(reply, sizeof(reply), "NICK %s\n", client_name);
            send(sock, reply, strlen(reply), 0);

            // Wait for the server's response to "NICK"
            memset(buffer, 0, sizeof(buffer));
            bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';  // Null-terminate the string

                if (strncmp(buffer, "ERROR ", 6) == 0) {
                    printf("%s", buffer);  // Print the error message
                    fflush(stdout);
                    close(sock);  // Close the connection
                    exit(1);      // Exit the program
                } else if (strcmp(buffer, "OK\n") == 0 || strcasecmp(buffer, "ok\n") == 0) {
                    printf("Handshake successful: %s", buffer);
                    fflush(stdout);
                } else {
                    printf("Unexpected response: %s", buffer);
                    fflush(stdout);
                }
            } else if (bytes_received == 0) {
                printf("Server disconnected\n");
                fflush(stdout);
                close(sock);
                exit(1);
            } else {
                perror("Error receiving server response");
                fflush(stderr);
                close(sock);
                exit(1);
            }
        }
    }
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
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR Connection failed");
        return -1;
    }

    // Perform handshake
    perform_handshake(sock, client_name);

    // Start the receive_messages thread after successful handshake
    pthread_t receive_thread;
    thread_args* args = malloc(sizeof(thread_args));
    args->sock = sock;
    strncpy(args->client_name, client_name, sizeof(args->client_name) - 1);
    args->client_name[sizeof(args->client_name) - 1] = '\0';

    if (pthread_create(&receive_thread, NULL, receive_messages, args) != 0) {
        perror("Error creating receive_messages thread");
        free(args);
        close(sock);
        return -1;
    }

    
    // Main thread for sending messages
    char message[256];
    while (1) {
        if (fgets(message, sizeof(message), stdin) == NULL) {
            break;
        }
        // message[strcspn(message, "\n")] = '\0'; // Remove newline character

        if (strcmp(message, "exit") == 0) {
            break;
        }

        if (strlen(message) > 255) {
            fprintf(stderr, "Error: Message length must not exceed 255 characters.\n");
            continue;
        }

        char full_message[300];
        snprintf(full_message, sizeof(full_message), "MSG %s", message);
        send(sock, full_message, strlen(full_message), 0);
        
    }
    pthread_join(receive_thread, NULL);
    close(sock);
    free(args);
    return 0;
}