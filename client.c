#define _GNU_SOURCE
#include "client.h"
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <ncurses.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "polycomm.h"
#include <sys/types.h>
#include <assert.h>
#include "util.h"
#include "network.h"
#include "cJSON/cJSON.h"


int username_id = 0;
int client_n = 0;
Client *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


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
        send_all(client->fd, &net_length, 4, &client->tx_state);
        send_all(client->fd, pr, length, &client->tx_state);

        cJSON_Delete(json);
        if(pr) free(pr);

        // this sends a request to the server to change the username
    } else if(strcmp(args[0], "!leave") == 0) {
        close(client->fd);
        endwin();
        exit(0);
    }
}


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
void *handle_chat(void *arg)
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
        send_all(client_fd, &net_length, 4, &client.tx_state);
        send_all(client_fd, pr, length, &client.tx_state);

        cJSON_free(pr);
        cJSON_Delete(message);
    }
    return NULL;
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
    Client *client = malloc(sizeof(Client));
    client->fd = client_fd;
    init_client(client);


    if(handshake_client(client_fd, &client->tx_state, &client->rx_state) == -1) {
        free(client);
        return;
    }

    freeaddrinfo(res);
    printf("\033[2J");

    pthread_t chat_thread;
    pthread_create(&chat_thread, NULL, handle_chat, client);
    pthread_detach(chat_thread);

    init_ncurses();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_CYAN, -1);
    while(1) {
        uint32_t net_length_u;
        recv_all(client_fd, &net_length_u, 4, &client->rx_state);

        uint32_t net_length = ntohl(net_length_u);

        char *json = (char *) malloc(net_length+1);
        if(!json) {
            puts("Malloc failed.");
            return;
        }

        json[net_length] = '\0';
        size_t n = recv_all(client_fd, json, net_length, &client->rx_state);
        if(n == 0) {
            close(client_fd);
            free(json);
        }

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
        }
    }
    endwin();
}
