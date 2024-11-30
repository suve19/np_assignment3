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
    char recv_buffer[BUFFER_SIZE];  // Buffer for incoming data
    int recv_buffer_length;         // Length of data in recv_buffer
} Client;

Client clients[MAX_CLIENTS];
int num_clients = 0;

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

// Add message to client's queue
void add_message_to_queue(Client *client, const char *message) {
    if ((client->queue_end + 1) % MAX_CLIENTS != client->queue_start) {
        strncpy(client->message_queue[client->queue_end], message, BUFFER_SIZE - 1);
        client->message_queue[client->queue_end][BUFFER_SIZE - 1] = '\0';  // Ensure null termination
        client->queue_end = (client->queue_end + 1) % MAX_CLIENTS;
    }
}

// Pop message from client's queue
int pop_message_from_queue(Client *client, char *message) {
    if (client->queue_start == client->queue_end) {
        return 0;
    }
    strncpy(message, client->message_queue[client->queue_start], BUFFER_SIZE - 1);
    message[BUFFER_SIZE - 1] = '\0';  // Ensure null termination
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

    // Format the message as "MSG <nickname> <message>"
    snprintf(formatted_message, sizeof(formatted_message), "MSG %s %s\n", sender_nickname, message);

    // Broadcast the formatted message to all clients except the sender
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].client_socket != sender_socket && clients[i].is_authenticated) {
            add_message_to_queue(&clients[i], formatted_message);
        }
    }
}

void process_client_message(Client *client, const char *message_line) {
    // Trim newline and carriage return characters
    char clean_message[BUFFER_SIZE];
    strncpy(clean_message, message_line, BUFFER_SIZE - 1);
    clean_message[BUFFER_SIZE - 1] = '\0';
    clean_message[strcspn(clean_message, "\r\n")] = '\0';

    if (!client->is_authenticated) {
        if (strncmp(clean_message, "NICK ", 5) == 0) {
            char *nickname = clean_message + 5;

            if (!validate_nickname(nickname)) {
                // Send error message to client
                if (strlen(nickname) > 12) {
                    fprintf(stderr, "Error: Nickname '%s' exceeds 12 characters.\n", nickname);
                    send(client->client_socket, "ERROR Nickname must not exceed 12 characters.\n", strlen("ERROR Nickname must not exceed 12 characters.\n"), 0);
                } else {
                    fprintf(stderr, "Error: Nickname '%s' contains invalid characters.\n", nickname);
                    send(client->client_socket, "ERROR Nickname can only contain alphanumeric characters and underscores.\n", strlen("ERROR Nickname can only contain alphanumeric characters and underscores.\n"), 0);
                }

                close(client->client_socket);
                // Remove client from the list
                for (int i = 0; i < num_clients; i++) {
                    if (clients[i].client_socket == client->client_socket) {
                        clients[i] = clients[num_clients - 1];
                        num_clients--;
                        break;
                    }
                }
                return;
            }

            // If valid, set nickname and authenticate
            strncpy(client->nickname, nickname, sizeof(client->nickname) - 1);
            client->nickname[sizeof(client->nickname) - 1] = '\0';  // Ensure null termination
            client->is_authenticated = 1;
            send(client->client_socket, "OK\n", strlen("OK\n"), 0);
            printf("Client nickname validated as '%s'\n", nickname);
        } else {
            send(client->client_socket, "ERROR Invalid command. Please provide your nickname using 'NICK <nickname>'.\n",
                 strlen("ERROR Invalid command. Please provide your nickname using 'NICK <nickname>'.\n"), 0);
        }
    } else {
        if (strncmp(clean_message, "MSG ", 4) == 0) {
            char *message_content = clean_message + 4;
            broadcast_message(client->client_socket, message_content);
        } else {
            fprintf(stderr, "Invalid message format from client %s\n", client->nickname);
            send(client->client_socket, "ERROR Invalid message format. Use 'MSG <message>'.\n",
                 strlen("ERROR Invalid message format. Use 'MSG <message>'.\n"), 0);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./server <ip>:<port>\n");
        return -1;
    }

    char *colon_pos = strchr(argv[1], ':');
    if (!colon_pos) {
        fprintf(stderr, "Invalid format. Use <ip>:<port>\n");
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
        return -1;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address.\n");
        close(server_socket);
        return -1;
    }
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        close(server_socket);
        return -1;
    }

    printf("Server listening on %s:%d...\n", ip, port);

    fd_set read_fds, write_fds;
    int max_sd = server_socket;

    while (1) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(server_socket, &read_fds);
        max_sd = server_socket;

        for (int i = 0; i < num_clients; i++) {
            int sd = clients[i].client_socket;
            FD_SET(sd, &read_fds);
            if (clients[i].queue_start != clients[i].queue_end) {
                FD_SET(sd, &write_fds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        int activity = select(max_sd + 1, &read_fds, &write_fds, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("Select error");
            break;
        }

        if (FD_ISSET(server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
            if (new_socket < 0) {
                perror("Accept failed");
                continue;
            }

            if (num_clients < MAX_CLIENTS) {
                clients[num_clients].client_socket = new_socket;
                clients[num_clients].queue_start = 0;
                clients[num_clients].queue_end = 0;
                clients[num_clients].is_authenticated = 0;
                clients[num_clients].recv_buffer_length = 0;

                char client_ip[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN) == NULL) {
                    strcpy(client_ip, "Unknown");
                }
                printf("New client connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

                // Send HELLO 1\n to new client
                send(new_socket, "HELLO 1\n", strlen("HELLO 1\n"), 0);
                num_clients++;
            } else {
                fprintf(stderr, "Max clients reached. Connection rejected.\n");
                close(new_socket);
            }
        }

        for (int i = 0; i < num_clients; i++) {
            int client_socket = clients[i].client_socket;

            if (FD_ISSET(client_socket, &read_fds)) {
                char buffer[BUFFER_SIZE];
                int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytes_received <= 0) {
                    if (bytes_received == 0) {
                        printf("Client disconnected\n");
                    } else {
                        perror("recv failed");
                    }
                    close(client_socket);

                    clients[i] = clients[num_clients - 1];
                    num_clients--;
                    i--;
                    continue;
                } else {
                    // Append received data to client's recv_buffer
                    if (clients[i].recv_buffer_length + bytes_received < BUFFER_SIZE) {
                        memcpy(clients[i].recv_buffer + clients[i].recv_buffer_length, buffer, bytes_received);
                        clients[i].recv_buffer_length += bytes_received;
                        clients[i].recv_buffer[clients[i].recv_buffer_length] = '\0';  // Null-terminate

                        // Process complete messages
                        char *line_end;
                        while ((line_end = strchr(clients[i].recv_buffer, '\n')) != NULL) {
                            size_t line_length = line_end - clients[i].recv_buffer + 1;
                            char message_line[BUFFER_SIZE];
                            memcpy(message_line, clients[i].recv_buffer, line_length);
                            message_line[line_length] = '\0';

                            // Shift the remaining data in recv_buffer
                            memmove(clients[i].recv_buffer, clients[i].recv_buffer + line_length, clients[i].recv_buffer_length - line_length);
                            clients[i].recv_buffer_length -= line_length;
                            clients[i].recv_buffer[clients[i].recv_buffer_length] = '\0';

                            // Process the message_line
                            process_client_message(&clients[i], message_line);
                        }
                    } else {
                        fprintf(stderr, "Buffer overflow for client %s\n", clients[i].nickname);
                        // Optionally disconnect the client
                        send(client_socket, "ERROR Buffer overflow. Disconnecting.\n", strlen("ERROR Buffer overflow. Disconnecting.\n"), 0);
                        close(client_socket);

                        clients[i] = clients[num_clients - 1];
                        num_clients--;
                        i--;
                        continue;
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
