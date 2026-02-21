#define _GNU_SOURCE
#include <pthread.h>
#include <netdb.h>
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
int username_id = 0;

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

    snprintf(client->username, USER_MAXLEN, "user_%d", username_id);

    clients[client_n] = client;
    client_n++;
    username_id++;

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
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i<client_n; i++) {
        send_all(clients[i]->fd, buf, len);
    }
    pthread_mutex_unlock(&clients_mutex);
}

static void handle_command(char *input, Client *client)
{
    char *args[6];
    int argn = 0;
    char input_copy[INPUT_MAX];
    strncpy(input_copy, input, INPUT_MAX);

    char *token = strtok(input_copy, " ");
    while(token != NULL && argn < 6) {
        args[argn++] = token;
        token = strtok(NULL, " ");
    }

    if(argn == 0) return;
    if(strcmp(args[0], "!changeusername") == 0) {
        if(argn < 2) {
            return;
        }
        cJSON *json = cJSON_CreateObject();
        if(!json) {
            return;
        }

        cJSON_AddStringToObject(json, "request", "username");
        cJSON_AddStringToObject(json, "target", args[1]);

        char *pr = cJSON_PrintUnformatted(json);
        if(!pr) return;
        uint32_t length = strlen(pr);
        uint32_t net_length = htonl(length);
        send_all(client->fd, &net_length, 4);
        send_all(client->fd, pr, length);

        cJSON_Delete(json);
        if(pr) free(pr);

        // this sends a request to the server to change the username
    }
}

static void *handle_chat(void *arg)
{
    Client client = *(Client *) arg;
    int client_fd = client.fd;

    while(1) {
        char buf[INPUT_MAX];
        werase(input_win);
        mvwprintw(input_win, 0, 0, "> ");
        wrefresh(input_win);

        wgetnstr(input_win, buf, INPUT_MAX - 1);
        wrefresh(output_win);

        if(buf[0] == '\0') {
            continue;
        }

        if(buf[0] == '!') {
            handle_command(buf, &client);
            continue;
        }

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

    cJSON *left = cJSON_CreateObject();
    char left_msg[64];
    snprintf(left_msg, sizeof(left_msg), "%s left", client->username);
    cJSON_AddStringToObject(left, "message", left_msg);
    cJSON_AddStringToObject(left, "author", "[SERVER]");
    char *pr = cJSON_PrintUnformatted(left);

    uint32_t length = strlen(pr);
    uint32_t net_length = htonl(strlen(pr));
    broadcast_clients(&net_length, 4);
    broadcast_clients(pr, length);


    if (client->username) {
        free(client->username);
        client->username = NULL;
    }

    close(client->fd);
    free(client);
    if(pr) {
        free(pr);
    }
    cJSON_Delete(left);
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
            if(!json) continue;
            json[net_length] = '\0';

            if(recv_all(client_fd, json, net_length) == 0) {
                disconnect_client(client);
                return NULL;
            }

            cJSON *parsed_json = cJSON_Parse(json);
            if(!parsed_json) {
                free(json);
                continue;
            }

            cJSON *message = cJSON_GetObjectItemCaseSensitive(parsed_json, "message");
            cJSON *request = cJSON_GetObjectItemCaseSensitive(parsed_json, "request");
            if(message && strcmp(message->valuestring, "")) {
                cJSON_AddStringToObject(parsed_json, "author", client->username);

                char *pr_j = cJSON_PrintUnformatted(parsed_json);
                printf("%s: %s\n", client->username, message->valuestring);

                uint32_t length = strlen(pr_j);
                uint32_t net_length_j = htonl(strlen(pr_j));
                broadcast_clients(&net_length_j, 4);
                broadcast_clients(pr_j, length);
                if(pr_j) {
                    free(pr_j);
                }
            } else if(request && strcmp(request->valuestring, "")) {
                cJSON *username = cJSON_GetObjectItemCaseSensitive(parsed_json, "target");
                if(!username) {
                    if(json) {
                        free(json);
                        cJSON_Delete(parsed_json);
                    }
                    continue;
                }

                cJSON *changed = cJSON_CreateObject();
                char change_msg[128];
                snprintf(change_msg, sizeof(change_msg), "%s changed their username to %s", client->username, username->valuestring);
                cJSON_AddStringToObject(changed, "message", change_msg);
                cJSON_AddStringToObject(changed, "author", "[SERVER]");
                char *pr = cJSON_PrintUnformatted(changed);

                uint32_t length = strlen(pr);
                uint32_t net_length = htonl(strlen(pr));
                broadcast_clients(&net_length, 4);
                broadcast_clients(pr, length);

                if(pr) free(pr);
                cJSON_Delete(changed);
                free(client->username);
                client->username = strdup(username->valuestring);
            }
            cJSON_Delete(parsed_json);
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

            cJSON *joined = cJSON_CreateObject();
            char join_msg[64];
            snprintf(join_msg, sizeof(join_msg), "%s joined", client->username);
            cJSON_AddStringToObject(joined, "message", join_msg);
            cJSON_AddStringToObject(joined, "author", "[SERVER]");
            char *pr = cJSON_PrintUnformatted(joined);

            uint32_t length = strlen(pr);
            uint32_t net_length = htonl(strlen(pr));
            broadcast_clients(&net_length, 4);
            broadcast_clients(pr, length);

            pthread_create(&client_thread, NULL, client_manage, client);
            pthread_detach(client_thread);
            if(pr) {
                free(pr);
            }
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
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(ip, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        return;
    }

    if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error.");
        return;
    }
    if (connect(client_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("Failed to connect.");
        freeaddrinfo(res);
        return;
    }

    freeaddrinfo(res);
    printf("\033[2J");

    Client *client = malloc(sizeof(Client));
    client->fd = client_fd;
    init_client(client);

    pthread_t chat_thread;
    pthread_create(&chat_thread, NULL, handle_chat, client);
    pthread_detach(chat_thread);

    init_ncurses();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_CYAN, -1);
    while(1) {
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
            if(!parsed_json) {
                continue;
            }

            cJSON *message = cJSON_GetObjectItemCaseSensitive(parsed_json, "message");
            cJSON *author = cJSON_GetObjectItemCaseSensitive(parsed_json, "author");
            if((message && author) && (cJSON_IsString(message) && cJSON_IsString(author))) {
                if(strcmp(author->valuestring, "[SERVER]") == 0) {
                    wattron(output_win, COLOR_PAIR(1));
                    wprintw(output_win, "%s: %s\n", author->valuestring, message->valuestring);
                    wattroff(output_win, COLOR_PAIR(1));
                    wrefresh(output_win);
                    continue;
                }
                wprintw(output_win, "%s: %s\n", author->valuestring, message->valuestring);
                wrefresh(output_win);
            }

            cJSON_Delete(parsed_json);
            free(json);
        } else if(n == 0) {
            close(client_fd);
        }
    }
    endwin();
}
