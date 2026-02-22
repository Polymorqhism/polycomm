#include <arpa/inet.h>
#include <sodium.h>
#include <pthread.h>
#define USER_MAXLEN 32
#define MAX_CLIENTS 5000

typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    char *username;
    crypto_secretstream_xchacha20poly1305_state tx_state;
    crypto_secretstream_xchacha20poly1305_state rx_state;
} Client;

extern int username_id;
extern int client_n;
extern Client *clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;


void init_client(Client *client);
void *handle_chat(void *arg);
