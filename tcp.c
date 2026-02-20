#include <pthread.h>
#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "cJSON/cJSON.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include "polycomm.h"
#include <sys/types.h>
#include <assert.h>
#include "util.h"
#include <errno.h>

#define USER_MAXLEN 32
#define MAX_CLIENTS 5000

typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    char *username;
} Client;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_n = 0;
Client *clients[MAX_CLIENTS];


void init_client(Client *client)
{
    pthread_mutex_lock(&clients_mutex);

    if (client_n >= MAX_CLIENTS) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    client->username = malloc(USER_MAXLEN);
    if (!client->username) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    snprintf(client->username, USER_MAXLEN, "user_%d", client_n);

    clients[client_n] = client;
    client_n++;

    pthread_mutex_unlock(&clients_mutex);
}

ssize_t send_all(int fd, const void *buffer, size_t len)
{
    size_t total_sent = 0;
    const char *buf = buffer;

    while (total_sent < len) {
        ssize_t n = send(fd, buf + total_sent, len - total_sent, 0);

        if (n > 0) {
            total_sent += n;
        } else if (n == 0) {
            return -1;
        } else {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
    }

    return total_sent;
}

ssize_t recv_all(int fd, void *buffer, size_t len)
{
    size_t total_received = 0;
    char *buf = buffer;

    while (total_received < len) {
        ssize_t n = recv(fd, buf + total_received, len - total_received, 0);

        if (n > 0) {
            total_received += n;
        }
        else if (n == 0) {
            return 0;
        }
        else {
            if (errno == EINTR)
                continue;

            return -1;
        }
    }

    return total_received;
}

static void broadcast_clients(void *buf, size_t len)
{
    for(int i = 0; i<client_n; i++) {
        pthread_mutex_lock(&clients_mutex);
        send_all(clients[i]->fd, buf, len);
        pthread_mutex_unlock(&clients_mutex);
    }
}

static void *handle_chat(void *arg)
{
    Client client = *(Client *) arg;
    int client_fd = client.fd;

    init_ncurses();
    while(1) {
        char buf[INPUT_MAX];
        werase(input_win);
        mvwprintw(input_win, 0, 0, "> ");
        wrefresh(input_win);

        wgetnstr(input_win, buf, INPUT_MAX - 1);
        wrefresh(output_win);

        cJSON *message = cJSON_CreateObject();
        if(cJSON_AddStringToObject(message, "message", buf) == NULL) {
            return NULL;
        }

        char *pr = cJSON_PrintUnformatted(message);

        uint32_t length = strlen(pr);
        uint32_t net_length = htonl(strlen(pr));
        send_all(client_fd, &net_length, 4);
        send_all(client_fd, pr, length);

        cJSON_free(pr);
        cJSON_Delete(message);
    }
    return NULL;
}

void disconnect_client(Client *client)
{
    if (!client) return;

    pthread_mutex_lock(&clients_mutex);

    // some magic stuff with removing the element only god knows how this works
    for (int i = 0; i < client_n; i++) {
        if (clients[i] == client) {
            clients[i] = clients[client_n - 1];
            clients[client_n - 1] = NULL;
            client_n--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    printf("Disconnecting client %s\n", client->username);

    if (client->username) {
        free(client->username);
        client->username = NULL;
    }

    close(client->fd);
    free(client);
}

void *client_manage(void *arg)
{
    Client *client = (Client *) arg;
    int client_fd = client->fd;

    while(1) {
        uint32_t net_length_u;
        ssize_t n = recv_all(client_fd, &net_length_u, 4);
        if(n <= 0) { // n == 0 can ONLY mean disconnect for some reason??
            disconnect_client(client);
            return NULL;
        }

        uint32_t net_length = ntohl(net_length_u);

        if(net_length < 1048576) { // <--- thats one MB
            char *json = (char *) malloc(net_length+1);
            json[net_length] = '\0';

            if(recv_all(client_fd, json, net_length) == 0) {
                disconnect_client(client);
                return NULL;
            }

            cJSON *parsed_json = cJSON_Parse(json);
            cJSON *message = cJSON_GetObjectItemCaseSensitive(parsed_json, "message");
            cJSON_AddStringToObject(parsed_json, "author", client->username);

            char *pr_j = cJSON_PrintUnformatted(parsed_json);
            printf("%s: %s\n", client->username, message->valuestring);

            uint32_t length = strlen(pr_j);
            uint32_t net_length_j = htonl(strlen(pr_j));
            broadcast_clients(&net_length_j, 4);
            broadcast_clients(pr_j, length);

            free(json);
        } else {
            puts("Max limit (1 MB) per receive exceeded, rejecting.");
        }
    }

    return NULL;
}

void handle_server_choice(void)
{
    char port[8];
    printf("Enter a port: ");
    fgets(port, sizeof(port), stdin);

    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed.");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed.");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    uint16_t port_int = atoi(port);
    address.sin_port = htons(port_int);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed.");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listening failed.");
        exit(EXIT_FAILURE);
    }

    while(1) {
        if ((client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen))< 0) {
            perror("Accepting failed.");
            exit(EXIT_FAILURE);
        } else {
            char client_ip[INET_ADDRSTRLEN];
            socklen_t addrlen = sizeof(addrlen);
            inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
            pthread_t client_thread;
            Client *client = malloc(sizeof(Client));
            strncpy(client->ip, client_ip, INET_ADDRSTRLEN);
            client->fd = client_fd;
            init_client(client);
            printf("Client accepted from %s with username %s.\n", client_ip, client->username);


            pthread_create(&client_thread, NULL, client_manage, client);
            pthread_detach(client_thread);
        }
    }
}



void handle_client_choice(void)
{
    char ip[32];
    char port[8];
    printf("Enter server IP: ");
    fgets(ip, sizeof(ip), stdin);
    printf("Enter server port: ");
    fgets(port, sizeof(port), stdin);
    ip[strcspn(ip, "\n")] = 0;
    port[strcspn(port, "\n")] = 0;
    printf("\nIP set as %s and port as %s.\n", ip, port);

    int client_fd;
    struct sockaddr_in server_addr;
    if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error.");
        return;
    }

    server_addr.sin_family = AF_INET;
    uint16_t port_int = atoi(port);
    server_addr.sin_port = htons(port_int);

    if(inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Bad IP.");
        return;
    }


    if(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect.");
        return;
    }

    puts("Connection successful.");
    printf("\033[2J");

    Client *client = malloc(sizeof(Client));
    client->fd = client_fd;
    init_client(client);

    pthread_t chat_thread;
    pthread_create(&chat_thread, NULL, handle_chat, client);
    pthread_detach(chat_thread);

    while(1) {
        // handling what server gives here:
        uint32_t net_length_u;
        recv_all(client_fd, &net_length_u, 4);

        uint32_t net_length = ntohl(net_length_u);

        char *json = (char *) malloc(net_length+1);
        if(!json) {
            puts("Malloc failed.");
            return;
        }

        json[net_length] = '\0';
        size_t n = recv_all(client_fd, json, net_length);
        if(n > 0) {
            cJSON *parsed_json = cJSON_Parse(json);
            cJSON *message = cJSON_GetObjectItemCaseSensitive(parsed_json, "message");
            cJSON *author = cJSON_GetObjectItemCaseSensitive(parsed_json, "author");
            wprintw(output_win, "%s: %s\n", author->valuestring, message->valuestring);
            wrefresh(output_win);

            cJSON_Delete(parsed_json);
            free(json);
        } else if(n == 0) {
            close(client_fd);
        }
    }
    endwin();
}
