#include <sys/types.h>
#include <stdint.h>
#include <sodium.h>

extern crypto_secretstream_xchacha20poly1305_state tx_state;
extern crypto_secretstream_xchacha20poly1305_state rx_state;

ssize_t send_all(int fd, const void *buffer, size_t len);
ssize_t recv_all(int fd, void *buffer, size_t len);

int handshake_server(int fd);
int handshake_client(int fd);
