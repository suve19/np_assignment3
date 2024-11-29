#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <errno.h>
#include <regex.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct {
    int client_socket;
    char nickname[13];  // Max 12 characters + null terminator
    char message_queue[MAX_CLIENTS][BUFFER_SIZE];
    int queue_start;
    int queue_end;
    int is_authenticated;
} Client;

Client clients[MAX_CLIENTS];
int num_clients = 0;

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

// Add message to client's queue
void add_message_to_queue(Client *client, const char *message) {
    if ((client->queue_end + 1) % MAX_CLIENTS != client->queue_start) {
        strcpy(client->message_queue[client->queue_end], message);
        client->queue_end = (client->queue_end + 1) % MAX_CLIENTS;
    }
}

// Pop message from client's queue
int pop_message_from_queue(Client *client, char *message) {
    if (client->queue_start == client->queue_end) {
        return 0;
    }
    strcpy(message, client->message_queue[client->queue_start]);
    client->queue_start = (client->queue_start + 1) % MAX_CLIENTS;
    return 1;
}

void broadcast_message(int sender_socket, const char *message) {
    char formatted_message[BUFFER_SIZE];

    // Find the sender's nickname
    const char *sender_nickname = "UNKNOWN";
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].client_socket == sender_socket) {
            sender_nickname = clients[i].nickname;
            break;
        }
    }

    // Format the message as "MSG <nickname>: <message>"
    snprintf(formatted_message, sizeof(formatted_message), "MSG %s %s", sender_nickname, message);

    // Broadcast the formatted message to all clients except the sender
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].client_socket != sender_socket && clients[i].is_authenticated) {
            add_message_to_queue(&clients[i], formatted_message);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./server <ip>:<port>\n");
        fflush(stderr);
        return -1;
    }

    char *colon_pos = strchr(argv[1], ':');
    if (!colon_pos) {
        fprintf(stderr, "Invalid format. Use <ip>:<port>\n");
        fflush(stderr);
        return -1;
    }

    char ip[100];
    int port;
    strncpy(ip, argv[1], colon_pos - argv[1]);
    ip[colon_pos - argv[1]] = '\0';
    port = atoi(colon_pos + 1);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        fflush(stderr);
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        fflush(stderr);
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        fflush(stderr);
        close(server_socket);
        return -1;
    }

    printf("Server listening on %s:%d...\n", ip, port);
    fflush(stdout);

    fd_set read_fds, write_fds;
    int max_sd = server_socket;

    while (1) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(server_socket, &read_fds);

        for (int i = 0; i < num_clients; i++) {
            FD_SET(clients[i].client_socket, &read_fds);
            if (clients[i].queue_start != clients[i].queue_end) {
                FD_SET(clients[i].client_socket, &write_fds);
            }
            if (clients[i].client_socket > max_sd) {
                max_sd = clients[i].client_socket;
            }
        }

        int activity = select(max_sd + 1, &read_fds, &write_fds, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("Select error");
            fflush(stderr);
            break;
        }

        if (FD_ISSET(server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
            if (new_socket < 0) {
                perror("Accept failed");
                fflush(stderr);
                continue;
            }

            if (num_clients < MAX_CLIENTS) {
                clients[num_clients].client_socket = new_socket;
                clients[num_clients].queue_start = 0;
                clients[num_clients].queue_end = 0;
                clients[num_clients].is_authenticated = 0;

                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                printf("New client connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
                fflush(stdout);

                // Send HELLO 1\n to new client
                send(new_socket, "HELLO 1\n", 9, 0);
                num_clients++;
            } else {
                fprintf(stderr, "Max clients reached. Connection rejected.\n");
                fflush(stderr);
                close(new_socket);
            }
        }

        for (int i = 0; i < num_clients; i++) {
            int client_socket = clients[i].client_socket;

            if (FD_ISSET(client_socket, &read_fds)) {
                char buffer[BUFFER_SIZE] = {0};
                int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytes_received <= 0) {
                    printf("Client disconnected\n");
                    fflush(stdout);
                    close(client_socket);

                    clients[i] = clients[num_clients - 1];
                    num_clients--;
                    i--;
                    continue;
                }

                buffer[bytes_received] = '\0';

                if (!clients[i].is_authenticated) {
                    if (strncmp(buffer, "NICK ", 5) == 0) {
                        char *nickname = buffer + 5;
                        nickname[strcspn(nickname, "\n")] = '\0';

                        if (!validate_nickname(nickname)) {
                            // Check the specific reason why nickname validation failed
                            if (strlen(nickname) > 12) {
                                fprintf(stderr, "Error: Nickname '%s' exceeds 12 characters.\n", nickname);
                                fflush(stderr);
                                send(client_socket, "ERROR Nickname must not exceed 12 characters.\n", 46, 0);
                            } else {
                                fprintf(stderr, "Error: Nickname '%s' contains invalid characters.\n", nickname);
                                fflush(stderr);
                                send(client_socket, "ERROR Nickname can only contain alphanumeric characters and underscores.\n", 72, 0);
                            }

                            close(client_socket);
                            clients[i] = clients[num_clients - 1];
                            num_clients--;
                            i--;
                            continue;
                        }

                        // If valid, set nickname and authenticate
                        strcpy(clients[i].nickname, nickname);
                        clients[i].is_authenticated = 1;
                        send(client_socket, "OK\n", 3, 0);
                        printf("Client nickname validated as '%s'\n", nickname);
                        fflush(stdout);
                    }
                } 
                else if (clients[i].is_authenticated) {
                    // printf("Received from client %s: %s\n", clients[i].nickname, buffer);
                    // fflush(stdout);
                    // Check if the message starts with "MSG "
                    if (strncmp(buffer, "MSG ", 4) == 0) {
                        char *message_content = buffer + 4; // Skip "MSG "
                        broadcast_message(client_socket, message_content); // Broadcast trimmed message
                    } else {
                        fprintf(stderr, "Invalid message format from client %s\n", clients[i].nickname);
                        fflush(stderr);
                    }
                }
            }

            if (FD_ISSET(client_socket, &write_fds)) {
                char message[BUFFER_SIZE];
                while (pop_message_from_queue(&clients[i], message)) {
                    send(client_socket, message, strlen(message), 0);
                }
            }
        }
    }

    close(server_socket);
    return 0;
}

