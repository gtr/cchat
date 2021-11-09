/*
 * Instructions for running:
 * =========================================
 * 
 * compile:
 *      $ make
 * run:
 *      $ ./client username [options]
 * clean:
 *      $ make clean
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>

#define LISTENQ 5
#define MAXLINE 4096
#define TRUE    1
#define FALSE   0
#define ZERO    0

static char* USERNAME;
static int PORT = 13001;
static char* IP = "127.0.0.1";

/* prints usage message */
void printUsage(char* argv[]) {
    printf("Usage:\n");
    printf("  %s username [options]\n\n", argv[0]);
    printf("Options:\n");
    printf("  --ip\t\tthe IP address of the server to connect to\n");
    printf("  --port\tthe port number to run on\n");
    exit(1);
}

/*
 * parseCommandLine parses the command line args to set username and port
 */
void parseCommandLine(int argc, char* argv[], char** username, int* port, char** ip) {
    if (argc == 2 || argc == 4 || argc == 6) {
        if (strcmp(argv[1], "--port") == 0 || (strcmp(argv[1], "--ip") == 0)) {
            printUsage(argv);
        } else {
            *username = argv[1];
        } 
        for (int i = 2; i < argc; i++, i++) {
            if (strcmp(argv[i], "--port") == 0) {
                *port = atoi(argv[i + 1]);
            } else if (strcmp(argv[i], "--ip") == 0) {
                *ip = argv[i + 1];
            } else {
                printUsage(argv);
            }
        }
    } else {
        printUsage(argv);
    }
}

/*
 * sendMessage reads data from src and writes it to dst.
 * 
 * src:         the source file descriptor to read from
 * dst:         the destination file descriptor to write to
 * read_set:    the set of file descriptors to block on
 *
 * returns:     an int 0 if client remains connected, -1 otherwise.
 */
int sendMessage(int src, int dst, fd_set read_set) {
    // Initialize buffer.
    char buffer[MAXLINE + 1];
    memset(buffer, ZERO, sizeof(buffer));
    

    // Read from src and store into buffer while handling read errors.
    if (read(src, buffer, MAXLINE) < 0) {
        if (src == STDIN_FILENO) {
            // If we fail to read from stdin, exit.
            perror("Error reading from stdin");
            exit(1);
        } else {
            // If we fail to read from socket, remove from the read file 
            // descriptor set and close the connection.
            perror("Error reading from socket");
            FD_CLR(src, &read_set);
            close(src);
            return -1;
        }
    }

    // Handles the case where the user decides to quit the chat room.
    if (strcmp(buffer, "quit()\n") == 0) {
        exit(0);
    }

    // Write the buffer to the destination.
    if (write(dst, buffer, strlen(buffer)) < 0) {
        perror("Error: Failed to write to destination"); 
    }

    return 0;
}

/*
 * startClient starts the client.
 */
void startClient() {
    // Create the client socket.
    int client_sockfd = socket(AF_INET, SOCK_STREAM, ZERO);
    if (client_sockfd < 0) {
        perror("Error opening socket.");
        exit(1);
    }

    // Initialize server address.
    struct sockaddr_in server_addr;
    memset(&server_addr, ZERO, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert the IP address string and convert to a network integer.
    if (inet_pton(AF_INET, IP, &server_addr.sin_addr) < 0) {
        perror("Error converting to network integer.");
        exit(1);
    }

    // Connect to the server.
    if (connect(client_sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting");
        exit(1);
    }

    if (write(client_sockfd, USERNAME, strlen(USERNAME)) < 0) {
		perror("Error: Failed to write to client"); 
	}

    int connected = TRUE;
    while (connected) {
        fd_set read_set, write_set;

        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_SET(STDIN_FILENO, &read_set);    // Standard in file descriptor
        FD_SET(client_sockfd, &read_set);   // Client connection file descriptor

        int max_fds = client_sockfd + 1;    // Next available file descriptor

        int fds = select(max_fds, &read_set, &write_set, NULL, NULL);
        if (fds < 0) {
            perror("Select failed");
            exit(1);
        }
        if (fds == 0) {
            perror("Select timed out");
            break;
        }

        // Check if standard input is available.
        if (FD_ISSET(STDIN_FILENO, &read_set)) {
            sendMessage(STDIN_FILENO, client_sockfd, read_set);
        }
        // Check if client socket input is available.
        if (FD_ISSET(client_sockfd, &read_set)) {
            if (sendMessage(client_sockfd, STDOUT_FILENO, read_set) < 0) {
                connected = FALSE;
            }
        }
    }
}

void printInformation() {
    printf("==================================\n");
    printf("ip      : %s\n", IP);
    printf("port    : %d\n", PORT);
    printf("username: %s\n", USERNAME);
    printf("==================================\n");
    printf("enter `quit()` to quit\n\n");
}

int main(int argc, char* argv[]) {
    // Parse the command line arguments.
    parseCommandLine(argc, argv, &USERNAME, &PORT, &IP);
    // Display server information.
    printInformation();
    // Start the client.
    startClient();
}
