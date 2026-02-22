#include <sys/types.h>
#include <stdint.h>
#include <sodium.h>

ssize_t recv_all(int fd, void *buffer, size_t len, crypto_secretstream_xchacha20poly1305_state *rx);
ssize_t send_all(int fd, const void *buffer, size_t len, crypto_secretstream_xchacha20poly1305_state *tx);

int handshake_client(int fd, crypto_secretstream_xchacha20poly1305_state *tx, crypto_secretstream_xchacha20poly1305_state *rx);
int handshake_server(int fd, crypto_secretstream_xchacha20poly1305_state *tx, crypto_secretstream_xchacha20poly1305_state *rx);
