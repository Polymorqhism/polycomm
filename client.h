#include <arpa/inet.h>
#include <pthread.h>
#define USER_MAXLEN 32
#define MAX_CLIENTS 5000

typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    char *username;
} Client;

extern int username_id;
extern int client_n;
extern Client *clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;


void init_client(Client *client);
void *handle_chat(void *arg);
