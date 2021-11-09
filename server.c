/*
 * Instructions for running:
 * =========================================
 * 
 * compile:
 *      $ make
 * run:
 *      $ ./server [options]
 * clean:
 *      $ make clean
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>

#define LISTENQ 5
#define MAX_USERNAME 20
#define MAX_MESSAGES 100    // Arbitrary number
#define MAX_CLIENTS 12      // Arbitrary number
#define BUFFER_SIZE 256
#define TRUE 1
#define ZERO 0

static char* USERNAME = "server";   // The username for the server (deprecated)
static int PORT = 13001;            // The default port number for the server

// Messages
const char* hello = " has entered the chat.\n";
const char* goodbye = " has left the chat.\n";

void serverLog(char* msg) {
    printf("[INFO] %s\n", msg);
}

/* 
 * -------------------------------------------------
 * Client data structure 
 * -------------------------------------------------
 */
typedef struct {
    int  id;                        // The Id of the client (queue position).
    int  sockfd;                    // The socket fd of the client.
    char username[MAX_USERNAME];    // The username of the client.
} client_t;

/*
 * clientInit initializes the client data structure.
 */
client_t *clientInit(int fd, char* username) {
    client_t *client = (client_t*) malloc(sizeof(client_t));
    client->id = 0; // Will be fixed later.
    client->sockfd = fd;
    strcpy(client->username, username);
    return client;
}

/* 
 * -------------------------------------------------
 * Client queue data structure 
 * -------------------------------------------------
 */
typedef struct {
    int count;                      // The number of messages present.
    client_t* data[MAX_CLIENTS];    // The array of clients.
    pthread_mutex_t mutex;          // Mutex for the data struture.
    pthread_cond_t cond;            // Conditional for the data structure.
} client_queue;

client_queue* clients;       // Global variable for client queue.

/*
 * clientQueueInit initializes the client_queue data structure.
 */
void clientQueueInit() {
    clients = (client_queue*) malloc(sizeof(client_queue));
    clients->count = 0;
    pthread_mutex_init(&clients->mutex, NULL);
}

/* For testing pusposes. Feel free to ignore... */
void clientQueuePrintClients(client_queue* q) {
    pthread_mutex_lock(&q->mutex);
    printf("========\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (q->data[i] == NULL) {
            printf("NULL\n");
        } else {
            printf("username: %s\n", q->data[i]->username);
        }
    }
    printf("========\n");
    pthread_mutex_unlock(&q->mutex);
}

/*
 * clientQueueAddClient adds a client to the client_queue data structure.
 */
void clientQueueAddClient(client_queue* q, client_t* c) {
    serverLog("clientQueueAddClient");
    if (q->count == MAX_CLIENTS) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    pthread_mutex_lock(&q->mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (q->data[i] == NULL) {
            c->id = i;      // Record the position of the client as the id.
            q->data[i] = c; // Save the client to the queue.
            q->count++;     // Increase the count of the queue.
            break;
        }
    }
    pthread_mutex_unlock(&q->mutex);
}

/*
 * clientQueuePopClient pops a client from the client_queue data structure.
 */
void clientQueuePopClient(client_queue* q, client_t* c) {
    pthread_mutex_lock(&q->mutex);
    q->data[c->id] = NULL;  // Erase the client from the queue.
    q->count--;             // Decrease the count of the queue.
    free(c);                // De-allocate from the heap.
    pthread_mutex_unlock(&q->mutex);
}

/* 
 * -------------------------------------------------
 * Message queue data structure 
 * -------------------------------------------------
 */
typedef struct {
    int count;                      // The number of messages present.
    char* messages[MAX_MESSAGES];   // The array of messages.
    int ids[MAX_MESSAGES];          // The array of corresponding ids.
    pthread_mutex_t mutex;          // Mutex for the data struture.
    pthread_cond_t cond;            // Conditional for the data structure.
} message_queue;

message_queue* messages;     // Global variable for messages queue.

/*
 * messageQueueInit initializes the message_queue data structure.
 */
void messageQueueInit() {
    messages = (message_queue*) malloc(sizeof(message_queue));
    pthread_mutex_init(&messages->mutex, NULL);
    pthread_cond_init(&messages->cond, NULL);
}

/*
 * messageQueueAddMessage adds a message to the message_queue data structure.
 * 
 * q    the message queue
 * msg  the message to be sent out
 * id   the ID of the user who sent this message
 */
void messageQueueAddMessage(message_queue* q, char* msg, int id) {
    pthread_mutex_lock(&q->mutex);
    // if (q->count == MAX_MESSAGES) {
    //     pthread_cond_wait(&q->cond, &q->mutex);
    // }
    for (int i = 0; i < MAX_MESSAGES; i++) {
        if (q->messages[i] == NULL) {
            q->messages[i] = msg;   // Save the message
            q->ids[i] = id;         // Save the ID of the user 
            q->count++;             // Increase the count
            break;
        }
    }
    // Let the conditional know that there is a message to broadcast.
    pthread_cond_signal(&q->cond); 
    pthread_mutex_unlock(&q->mutex);
}

/*
 * handleBroadcast handles when a message needs to be broadcasted to all 
 * online users. It waits until there are messages to be sent out and 
 * sends every message to every user.
 */
void* handleBroadcast(void* arg) {
    while (TRUE) {
        // pthread_mutex_lock(&messages->mutex);
        // Wait until there are messages to be sent out.
        while(messages->count == 0) {
            pthread_cond_wait(&messages->cond, &messages->mutex);
        }
    
        // When there are messages, pop them.
        for (int i = 0; i < MAX_MESSAGES; i++) {
            // Go through every message...
            if (messages->messages[i] != NULL) {
                char* msg = messages->messages[i];
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    // Go through every client...
                    if (clients->data[j] != NULL) {
                        client_t user = *clients->data[j];
                        // I tried making it so that the message would be sent 
                        // to everyone except the user who sent it but it 
                        // doesn't work:
                        // if (user.id != *messages->ids) {
                        // }
                        
                        // And write every message to every client.
                        if (write(user.sockfd, msg, strlen(msg)) < 0) {
                            perror("Failed to write to client");
                        }
                    }
                }
                // Remove message from the queue.
                messages->count--;
                messages->messages[i] = NULL;
            }
        }
    }
    pthread_mutex_unlock(&messages->mutex);
    pthread_mutex_unlock(&clients->mutex);
}


/* prints usage message. */
void printUsage(char* argv[]) {
    printf("Usage:\n");
    printf("  %s [options]\n\n", argv[0]);
    printf("Options:\n");
    printf("  --port\tthe port number to run on\n");
    exit(1);
}

/*
 * parseCommandLine parses the command line args to set username and port.
 */
void parseCommandLine(int argc, char* argv[], int* port) {
    if (argc == 1) {
        return;
    } else if (argc == 3) {
        if (strcmp(argv[1], "--port") == 0) {
            *port = atoi(argv[2]);
        } else {
            printUsage(argv);
        } 
    } else {
        printUsage(argv);
    }
}

void printInformation() {
    printf("==================================\n");
    printf("ip      : 127.0.0.1\n");
    printf("port    : %d\n", PORT);
    printf("username: %s\n", USERNAME);
    printf("==================================\n\n");
}

/*
 * welcomeClient handles the situation when a new client joins the chat room,
 * broadcasting that the user has joined, and creates a list of all online 
 * users and sends it to the client's socket file descriptor.
 *
 * client   the new client 
 */
void welcomeClient(client_t *client) {
    // Broadcast that a new user has joined.
    char message[BUFFER_SIZE];
    memset(message, 0, sizeof(message));
    strncat(message, client->username, strlen(client->username));
    strncat(message, hello, strlen(hello));
    messageQueueAddMessage(messages, message, client->id);

    // Initialize the buffer that contains all the online users.
    char onlineStatus[BUFFER_SIZE];
    memset(onlineStatus, 0, sizeof(onlineStatus));
    strcat(onlineStatus, "Online: \n");
    
    // Add online users to the buffer.
    pthread_mutex_lock(&clients->mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients->data[i] != NULL) {
            char* name = clients->data[i]->username;
            strncat(onlineStatus, name, strlen(name));
            strncat(onlineStatus, "\n", 1);
        }
    }
    // Send list of online users to the new user.
    if (write(client->sockfd, onlineStatus, strlen(onlineStatus)) < 0) {
        perror("Failed to write to client!!");
    }
    pthread_mutex_unlock(&clients->mutex);
}

/*
 * dismissClient dismisses the client and broadcasts that the user has left.
 *
 * client   the dismissed client 
 */
void dismissClient(client_t *client) {
    // Broadcast message that the user has left.
    char message[BUFFER_SIZE];
    memset(message, 0, sizeof(message));
    strncat(message, client->username, strlen(client->username));
    strncat(message, goodbye, strlen(goodbye));
    messageQueueAddMessage(messages, message, -1);
    
    // Close connection and pop from client queue.
    close(client->sockfd);
    clientQueuePopClient(clients, client);
}


/*
 * handleNewClient handles the event when a new client enters the chat room. It
 * creates a new client struct with the username and socket fd and blocks until
 * a message is read from the socket fd. It then builds a message and 
 * broadcasts it to all the users that are currently online. 
 */
void* handleNewClient(void* arg) {
    // Get the socket file descriptor and read the client's username.
    int* client_socket = arg;
    char temp[BUFFER_SIZE + 1];
    memset(temp, 0, sizeof(temp));
    if (read(*client_socket, temp, BUFFER_SIZE) < 0) {
        perror("Failed to read!!");
    }

    // Initialize a new client.
    client_t *client = clientInit(*client_socket, temp);

    // Add new client to client queue and welcome the client.
    clientQueueAddClient(clients, client);
    welcomeClient(client);

    // Initialize the buffer to read from the client.
    char buffer[BUFFER_SIZE];
    memset(&buffer, 0, sizeof(buffer));
    
    // Initialize the buffer containing the message to be sent out.
    char message[BUFFER_SIZE];
    memset(&message, 0, sizeof(message));

    // Block until we read a message from this client's socket file descriptor.
    while (read(client->sockfd, buffer, BUFFER_SIZE) > 0) {
        // Handles the case where the user decides to quit the chat room.
        if (strcmp(buffer, "quit()\n") == 0) {
            break;
        }
        // Build the full message with the username prepended.
        memset(&message, 0, sizeof(message));
        strncpy(message, client->username, strlen(client->username));
        strncat(message, ": ", strlen(": "));
        strncat(message, buffer, strlen(buffer));
        
        // Add the full message to the queue.
        messageQueueAddMessage(messages, message, client->id);
        pthread_cond_signal(&messages->cond);
        memset(&buffer, 0, sizeof(buffer));
    }

    // Dismiss client.
    serverLog("dismissing");
    dismissClient(client);
    pthread_exit(0);
}

/*
 * startServer starts the server. It sets up a socket for the server, binds it,
 * and listens until it connects to a client. 
 */
void startServer() {
    // Create the serve socket.
    int server_socket = socket(AF_INET, SOCK_STREAM, ZERO);
    if (server_socket < 0) {
        perror("Could not create socket");
        exit(1);
    }

    // Set up sockaddr_in, zero it (adapted from lecture notes).
    struct sockaddr_in server_addr;
    memset(&server_addr, ZERO, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port        = htons(PORT);

    // Bind the socket.
    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Could not bind socket");
        exit(1);
    }
    // Listen with server_socket and set the queue length.
    if (listen(server_socket, LISTENQ) < 0) {
        perror("Could not bind socket");
        exit(1);
    }

    serverLog("Server started");

    // Create the thread responsible for broadcasting all messages.
    pthread_t broadcast_tid;
    pthread_create(&broadcast_tid, NULL, handleBroadcast, NULL);

    // Enter the infinite loop.
    while (TRUE) {
        // Block until we accept a client socket.
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            perror("Failed to accept the client connection.");
        }
        // pthread_mutex_lock(&clients->mutex);
        // Check if we have enough space to deal with the client.
        if (clients->count == MAX_CLIENTS) {
            serverLog("Maximum clients reached.");
            close(client_socket);
            continue;
        }
        // pthread_mutex_unlock(&clients->mutex);
        
        // Create the thread responsible for handling client requests.
        pthread_t client_tid;
        pthread_create(&client_tid, NULL, handleNewClient, &client_socket);
        pthread_detach(client_tid);
    }

    close(server_socket);
}

int main(int argc, char* argv[]) {
    // Parse the command line arguments.
    parseCommandLine(argc, argv, &PORT);
    // Initialize the client queue.
    clientQueueInit();
    // Initialize the message queue.
    messageQueueInit();
    // Display server information.
    printInformation();
    // Start the server.
    startServer();
}
