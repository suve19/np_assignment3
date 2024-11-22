#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <errno.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

// Structure to store a message queue for each client
typedef struct {
    int client_socket;
    char message_queue[MAX_CLIENTS][BUFFER_SIZE];
    int queue_start;
    int queue_end;
} Client;

// Global array of clients
Client clients[MAX_CLIENTS];
int num_clients = 0;

// Function to add a message to the client's message queue
void add_message_to_queue(Client *client, const char *message) {
    if ((client->queue_end + 1) % MAX_CLIENTS != client->queue_start) { // Check for queue overflow
        strcpy(client->message_queue[client->queue_end], message);
        client->queue_end = (client->queue_end + 1) % MAX_CLIENTS;
    }
}

// Function to pop a message from the client's message queue
int pop_message_from_queue(Client *client, char *message) {
    if (client->queue_start == client->queue_end) {
        return 0; // Queue is empty
    }
    strcpy(message, client->message_queue[client->queue_start]);
    client->queue_start = (client->queue_start + 1) % MAX_CLIENTS;
    return 1;
}

// Function to broadcast a message to all clients except the sender
void broadcast_message(int sender_socket, const char *message) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].client_socket != sender_socket) {
            add_message_to_queue(&clients[i], message);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./server <ip>:<port>\n");
        fflush(stderr);
        return -1;
    }

    // Parse IP and port from the argument
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

    // Create the server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        fflush(stderr);
        return -1;
    }

    // Define server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        fflush(stderr);
        close(server_socket);
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        fflush(stderr);
        close(server_socket);
        return -1;
    }

    printf("Server is listening on %s:%d...\n", ip, port);
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

        // Use select() to monitor sockets
        int activity = select(max_sd + 1, &read_fds, &write_fds, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("Select error");
            fflush(stderr);
            break;
        }

        // Handle new connections
        if (FD_ISSET(server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
            if (new_socket < 0) {
                perror("Accept failed");
                fflush(stderr);
                continue;
            }

            // Add new client to the clients array
            if (num_clients < MAX_CLIENTS) {
                clients[num_clients].client_socket = new_socket;
                clients[num_clients].queue_start = 0;
                clients[num_clients].queue_end = 0;
                num_clients++;

                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                printf("New client connected from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
                fflush(stdout);
            } else {
                fprintf(stderr, "Max clients reached. Connection rejected.\n");
                fflush(stderr);
                close(new_socket);
            }
        }

        // Handle communication with clients
        for (int i = 0; i < num_clients; i++) {
            int client_socket = clients[i].client_socket;

            // Check if client sent a message
            if (FD_ISSET(client_socket, &read_fds)) {
                char buffer[BUFFER_SIZE] = {0};
                int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytes_received <= 0) {
                    // Client disconnected
                    printf("Client disconnected\n");
                    fflush(stdout);
                    close(client_socket);

                    // Remove client from array
                    clients[i] = clients[num_clients - 1];
                    num_clients--;
                    i--;
                    continue;
                }

                buffer[bytes_received] = '\0';

                // Broadcast the message to other clients
                broadcast_message(client_socket, buffer);
            }

            // Check if client has messages to send
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
