/*
 * client.c -- TCP Socket Client
 * 
 * adapted from: 
 *   https://www.educative.io/answers/how-to-implement-tcp-sockets-in-c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

// Function to send a file from the client to the server
void writeFileToServer(int server_sock, const char* localFilePath, const char* remoteFilePath) {
    // Open the file in local machine
    FILE* file = fopen(localFilePath, "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Send remote file path to the server
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "WRITE %s\n", remoteFilePath);
    if (send(server_sock, buffer, strlen(buffer), 0) == -1) {
        perror("Error sending WRITE command");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Send the file data in localFilePath
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(server_sock, buffer, bytes_read, 0) == -1) {
            perror("Error sending file data");
            fclose(file);
            exit(EXIT_FAILURE);
        }
    }
    printf("Write to file successfully!\n");
    
    // Close the file and the socket
    fclose(file);
}

// Function to get a file from remote file system
void getFileFromServer(int server_sock, const char* remoteFilePath, const char* localFilePath) {
    // Send GET command and remote file path to the server
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "GET %s\n", remoteFilePath);
    if (send(server_sock, buffer, strlen(buffer), 0) < 0) {
        perror("Error sending GET command");
        exit(EXIT_FAILURE);
    }

    // Open the local file for writing
    FILE* file = fopen(localFilePath, "wb");
    if (file == NULL) {
        perror("Error opening local file");
        exit(EXIT_FAILURE);
    }

    // Receive the file data
    int bytesRead;
    while ((bytesRead = recv(server_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        // Check for the end-of-file transmission signal
        if (bytesRead == 3 && strncmp(buffer, "EOF", 3) == 0) {
            break; // End of file transmission
        }
        fwrite(buffer, 1, bytesRead, file);
    }
    printf("File received successfully!\n");

    // Close the file
    fclose(file);
}

void sendRemoveCommand(int server_sock, const char* remoteFilePath) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "RM %s\n", remoteFilePath);
    if (send(server_sock, buffer, strlen(buffer), 0) < 0) {
        perror("Error sending RM command");
        exit(EXIT_FAILURE);
    }
    printf("send remove file request successfully!\n");
}


int main(int argc, char *argv[])
{
    //setup connection
    int socket_desc;
    struct sockaddr_in server_addr;
    char server_message[2000], client_message[2000];

    // Clean buffers:
    memset(server_message,'\0',sizeof(server_message));
    memset(client_message,'\0',sizeof(client_message));

    // Create socket:
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_desc < 0){
    printf("Unable to create socket\n");
    return -1;
    }

    printf("Socket created successfully\n");

    // Set port and IP the same as server-side:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Send connection request to server:
    if(connect(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
    printf("Unable to connect\n");
    return -1;
    }
    printf("Connected with server successfully\n");

    // Usage of writeFileToServer function
    if (argc >= 3 && strcmp(argv[1], "WRITE") == 0) {
        const char* remoteFilePath = (argc == 4) ? argv[3] : argv[2];
        writeFileToServer(socket_desc, argv[2], remoteFilePath);
    } 
    // Usage of getFileFromServer function
    else if (argc >= 3 && strcmp(argv[1], "GET") == 0) {
        const char* localFilePath = (argc == 4) ? argv[3] : argv[2];
        getFileFromServer(socket_desc, argv[2], localFilePath);
    }
    // Usage of sendRemoveCommand function
    else if (argc == 3 && strcmp(argv[1], "RM") == 0) {
        sendRemoveCommand(socket_desc, argv[2]);
    }
    // Usage of LS function
    else if (argc == 3 && strcmp(argv[1], "LS") == 0) {
        char buffer[BUFFER_SIZE];
        snprintf(buffer, BUFFER_SIZE, "LS %s\n", argv[2]);
        send(socket_desc, buffer, strlen(buffer), 0);

        // Receive and print versioning information
        if (recv(socket_desc, buffer, BUFFER_SIZE, 0) > 0) {
            printf("Versioning Info: %s\n", buffer);
        }
}
    // Invalid command
    else {
        printf("Invalid command\n");
    }

    // Close the socket:
    close(socket_desc);

    return 0;
}

