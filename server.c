#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

// Global mutex for file operations
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// File metadata structure
typedef struct {
    char filename[BUFFER_SIZE];
    int version;
    char path[BUFFER_SIZE];
} FileMetadata;

// Global file table 
FileMetadata fileTable[100]; // size of 100
int fileTableSize = 0;

// Initialize the file table
void initializeFileTable() {
    fileTableSize = 0;
}

// Function to check and update file table for write operations
void updateFileTableForWrite(const char* filename) {
    bool fileExists = false;
    for (int i = 0; i < fileTableSize; i++) {
        if (strcmp(fileTable[i].filename, filename) == 0) {
            fileTable[i].version++;
            fileExists = true;
            break;
        }
    }

    if (!fileExists) {
        // Add new file to table
        strcpy(fileTable[fileTableSize].filename, filename);
        fileTable[fileTableSize].version = 1;
        strcpy(fileTable[fileTableSize].path, "path/to/versioned/files");
        fileTableSize++;
    }
}

void processRemoveCommand(int client_sock, const char* filePath) {
    if (remove(filePath) == 0) {
        send(client_sock, "File removed successfully\n", 26, 0);
    } else {
        perror("Error removing file");
        send(client_sock, "Error removing file\n", 21, 0);
    }
}

// Thread function to handle client
void *client_handler(void *socket_desc) {
    int client_sock = *(int *)socket_desc;
    free(socket_desc);

    char client_message[BUFFER_SIZE];

    printf("start receive client request \n");

    // Receiving the client's request
    while (1) {
        ssize_t read_size = recv(client_sock, client_message, BUFFER_SIZE - 1, 0);  // Leave space for null terminator
        if (read_size <= 0) {
            if (read_size == 0) {
                puts("Client disconnected");
            } else {
                perror("recv failed");
            }
            break;
        }
        client_message[read_size] = '\0';  // Null-terminate the message

        // Process client request WRITE command
        if (strncmp(client_message, "WRITE ", 6) == 0) {
            char* filePath = strtok(client_message + 6, "\n");

            pthread_mutex_lock(&file_mutex);  // Lock the mutex for file operation
            FILE* file = fopen(filePath, "wb");
            if (file == NULL) {
                perror("Error opening file");
                pthread_mutex_unlock(&file_mutex);
                continue;
            }
            updateFileTableForWrite(filePath);
            ssize_t bytesRead;
            // Read file data from client until there's no more data
            while ((bytesRead = recv(client_sock, client_message, BUFFER_SIZE, 0)) > 0) {
                fwrite(client_message, 1, bytesRead, file);
            }

            fclose(file);
            pthread_mutex_unlock(&file_mutex); 
            printf("File written successfully\n");
        }

        // Process the GET command
        else if (strncmp(client_message, "GET ", 4) == 0) {
            char* filePath = strtok(client_message + 4, "\n");
            pthread_mutex_lock(&file_mutex);  // Lock the mutex for file operation
            FILE* file = fopen(filePath, "rb");
            if (file == NULL) {
                perror("Error opening file");
                close(client_sock);
                continue; // Continue listening for other clients
            }

            ssize_t bytesRead;
            while ((bytesRead = fread(client_message, 1, BUFFER_SIZE, file)) > 0) {
                if (send(client_sock, client_message, bytesRead, 0) < 0) {
                    perror("Error sending file");
                    fclose(file);
                    close(client_sock);
                    // continue; // Continue listening for other clients
                }
            }

            fclose(file);
            pthread_mutex_unlock(&file_mutex);  // Unlock the mutex
            printf("File sent successfully\n");

            // Send an end-of-file transmission signal
            send(client_sock, "EOF", 3, 0);
        }

        // Process the RM command
        else if (strncmp(client_message, "RM ", 3) == 0) {
            char* filePath = strtok(client_message + 3, "\n");
            pthread_mutex_lock(&file_mutex);  // Lock the mutex for file operation
            processRemoveCommand(client_sock, filePath);
            pthread_mutex_unlock(&file_mutex); // Unlock the mutex
            printf("File removed successfully\n");
        }
    }

    // Close the client socket
    close(client_sock);
    return NULL;
}

int main(void) {
    //server start, construct the file table
    initializeFileTable();

    if (pthread_mutex_init(&file_mutex, NULL) != 0) {
    perror("Mutex init failed");
    return 1;
    }

    int socket_desc, client_sock;
    socklen_t client_size;
    struct sockaddr_in server_addr, client_addr;
    char client_message[BUFFER_SIZE];

    // Create socket:
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0) {
        printf("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");

    // Set port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Bind to the set port and IP:
    if (bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Couldn't bind to the port\n");
        return -1;
    }
    printf("Done with binding\n");

    // Listen for clients:
    if (listen(socket_desc, 1) < 0) {
        printf("Error while listening\n");
        return -1;
    }
    printf("Listening for incoming connections.....\n");

    // Server main loop
    while(1) {
        // Accept an incoming connection:
        client_size = sizeof(client_addr);
        client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);
        if (client_sock < 0) {
            perror("Accept failed");
            continue; // Continue listening for other clients
        }
        printf("Client connected\n");

        // Allocate memory for client socket
        int* new_sock = malloc(sizeof(int));
        if (new_sock == NULL) {
            perror("Failed to allocate memory for new socket");
            close(client_sock);
            continue;
        }
        *new_sock = client_sock;

        // Create a thread for each client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, (void*) new_sock) < 0) {
            perror("Could not create thread");
            free(new_sock);
            close(client_sock);
            continue;
        }
        // Optionally, detach the thread to allow it to clean up after itself
        pthread_detach(thread_id);
    }

    // Close the server socket
    close(socket_desc);

    // Destroy the mutex
    pthread_mutex_destroy(&file_mutex);

    return 0;
}
