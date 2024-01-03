#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

//#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

const char* get_file_extension(const char* filename) 
{
    const char* dot = strrchr(filename, '.');

    if (!dot || dot == filename) 
    {
        return "";
    }
    return dot + 1;
}

const char* get_content_type(const char* extension) 
{
    // Content type mapping
    if (strcmp(extension, "html") == 0 || strcmp(extension, "htm") == 0) 
    {
        return "text/html; charset=UTF-8";
    } else if (strcmp(extension, "js") == 0) 
    {
        return "application/javascript";
    } else if (strcmp(extension, "png") == 0) 
    {
        return "image/png";
    } else if (strcmp(extension, "jpg") == 0 || strcmp(extension, "jpeg") == 0) {
        return "image/jpeg";
    } else 
    {
        return "application/octet-stream";
    }
}

void serve_file(int clientSocket, const char* filePath) 
{

    //file in binary
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) 
    {
        // File not found
        const char* notFoundResponse = "HTTP/1.1 404 Not Found\r\n"
                                       "Content-Length: 16\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "\r\n"
                                       "File Not Found\r\n";
        write(clientSocket, notFoundResponse, strlen(notFoundResponse));
        return;
    }

    //determ file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    // dbcheck file
    fseek(file, 0, SEEK_SET);
    //allocste mem to store file stuff
    char* fileContent = malloc(fileSize);
    if (fileContent == NULL) 
    {
        //read file content
         //size_t bytesRead = fread(fileContent, 1, fileSize, file);
                   // fileContent[bytesRead] = '\0';
        fclose(file);//fclose(fileSize)
        perror("Error allocating memory for file content");
        exit(1);//ex(0)
    }

    fread(fileContent, 1, fileSize, file);
    fclose(file);

    const char* extension = get_file_extension(filePath);
    const char* contentType = get_content_type(extension);

    char response[BUFFER_SIZE];
    int responseLength = snprintf(response, BUFFER_SIZE,
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %ld\r\n"
        "Content-Type: %s\r\n"
        "\r\n",
        fileSize, contentType);
    
    write(clientSocket, response, responseLength);
    write(clientSocket, fileContent, fileSize);

    free(fileContent);
}
    //start hereeeeeee

int main(int argc, char* argv[]) 
{
    if (argc != 3) 
    {
        fprintf(stderr, "Usage: %s <port> <directory>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    char* directory = argv[2];

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) 
    {
        perror("Error in socket creation");
        exit(1);
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    //bind start here if im right
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) 
    {
        perror("Error in binding");
        exit(1);
    }
    //start listen if got connnection from client
    if (listen(serverSocket, 5) < 0) 
    {
        perror("Error in listen");
        exit(1);
    }

    fd_set readfds;
    int maxfd;
    int clientSockets[MAX_CLIENTS];
    memset(clientSockets, 0, sizeof(clientSockets));

    printf("Server listening on port %d...\n", port);

    while (1) 
    {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        maxfd = serverSocket;
        //add client socket to set so make sure correct
        for (int i = 0; i < MAX_CLIENTS; i++) 
        {
            int clientSocket = clientSockets[i];
            if (clientSocket > 0) 
            {
                FD_SET(clientSocket, &readfds);
                if (clientSocket > maxfd) 
                {
                    maxfd = clientSocket;
                }
            }
        }

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) 
        {
            perror("Error in select");
            exit(1);
        }

        if (FD_ISSET(serverSocket, &readfds)) 
        {
            struct sockaddr_in clientAddress;
            socklen_t clientAddressLength = sizeof(clientAddress);
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength);
            if (clientSocket < 0) 
            {
                perror("Error in accept");
                exit(1);
            }

            printf("New client connection, socket fd is %d, IP is : %s, port : %d\n",
                   clientSocket, inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));

            for (int i = 0; i < MAX_CLIENTS; i++) 
            {
                if (clientSockets[i] == 0) 
                {
                    clientSockets[i] = clientSocket;
                    break;
                }
            }
        }
        //check activity detect or not client socket
        for (int i = 0; i < MAX_CLIENTS; i++) 
        {
            int clientSocket = clientSockets[i];
            if (FD_ISSET(clientSocket, &readfds)) 
            {
                char buffer[BUFFER_SIZE];
                ssize_t bytesRead = read(clientSocket, buffer, BUFFER_SIZE - 1);
                if (bytesRead < 0) 
                {
                    perror("Error in reading from client");
                    exit(1);
                } else if (bytesRead == 0) {
                    // Client disconnected
                    printf("Client disconnected, socket fd is %d\n", clientSocket);
                    close(clientSocket);
                    clientSockets[i] = 0;
                } else 
                {
                    buffer[bytesRead] = '\0';
                    // Parse req  for file
                    char* requestLine = strtok(buffer, "\r\n");
                    char* requestMethod = strtok(requestLine, " \t");
                    char* requestPath = strtok(NULL, " \t");

                    if (requestPath != NULL) 
                    {
                        char filePath[BUFFER_SIZE];
                        snprintf(filePath, BUFFER_SIZE, "%s%s", directory, requestPath);

                        printf("File path: %s\n", filePath);

                        if (strcmp(requestMethod, "GET") == 0) 
                        {
                            serve_file(clientSocket, filePath);
                        }
                    }
                }
            }
        } 
        //free the allocated mem
                  //  free(fileContent);
    }
    

    close(serverSocket);

    return 0;
}

