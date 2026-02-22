#include <stdlib.h>
#include <sys/socket.h>
#include <sodium.h>
#include <errno.h>

crypto_secretstream_xchacha20poly1305_state tx_state;
crypto_secretstream_xchacha20poly1305_state rx_state;

ssize_t send_raw(int fd, const void *buffer, size_t len)
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

ssize_t recv_raw(int fd, void *buffer, size_t len)
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


ssize_t send_all(int fd, const void *buffer, size_t len)
{

    unsigned long long ciphertext_len;
    unsigned char *ciphertext = (unsigned char*) malloc(len + crypto_secretstream_xchacha20poly1305_ABYTES);
    if(!ciphertext) {
        return -1;
    }

    crypto_secretstream_xchacha20poly1305_push(&tx_state, ciphertext, &ciphertext_len, (const unsigned char *)buffer, len, NULL, 0, crypto_secretstream_xchacha20poly1305_TAG_MESSAGE);

    ssize_t sent = send_raw(fd, ciphertext, (size_t) ciphertext_len);

    free(ciphertext);
    return sent;
}

ssize_t recv_all(int fd, void *buffer, size_t len)
{
    unsigned long long decrypted_len;
    size_t ciphertext_len = len + crypto_secretstream_xchacha20poly1305_ABYTES;

    unsigned char *ciphertext = (unsigned char*) malloc(ciphertext_len);

    if(!ciphertext) {
        return -1;
    }

    ssize_t received_size = recv_raw(fd, ciphertext, ciphertext_len);
    if(received_size <= 0) {
        free(ciphertext);
        return -1;
    }

    if(crypto_secretstream_xchacha20poly1305_pull(&rx_state, (unsigned char *)buffer, &decrypted_len, NULL, ciphertext, ciphertext_len, NULL, 0) == -1) { /* tampered or corrupted */
        free(ciphertext);
        return -1;
    }

    return (ssize_t) decrypted_len;
}

int handshake_server(int fd)
{
    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
    unsigned char server_sk[crypto_kx_SECRETKEYBYTES];

    crypto_kx_keypair(server_pk, server_sk);
    send_raw(fd, server_pk, crypto_kx_PUBLICKEYBYTES);

    unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];
    recv_raw(fd, client_pk, crypto_kx_PUBLICKEYBYTES);

    unsigned char rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char tx[crypto_kx_SESSIONKEYBYTES];
    if(crypto_kx_server_session_keys(rx, tx, server_pk, server_sk, client_pk) == -1) {
        return -1; // fail path
    }
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_init_push(&tx_state, header, tx);
    send_raw(fd, header, sizeof(header));
    recv_raw(fd, header, sizeof(header));

    if(crypto_secretstream_xchacha20poly1305_init_pull(&rx_state, header, rx) == -1) {
        return -1;
    }
    return 0;
}

int handshake_client(int fd)
{
    unsigned char client_sk[crypto_kx_SECRETKEYBYTES];
    unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];
    crypto_kx_keypair(client_pk, client_sk);

    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
    recv_raw(fd, server_pk, crypto_kx_PUBLICKEYBYTES);
    send_raw(fd, client_pk, crypto_kx_PUBLICKEYBYTES);

    unsigned char rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char tx[crypto_kx_SESSIONKEYBYTES];
    if(crypto_kx_client_session_keys(rx, tx, client_pk, client_sk, server_pk) == -1) {
        return -1;
    }

    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    recv_raw(fd, header, sizeof(header));
    if(crypto_secretstream_xchacha20poly1305_init_pull(&rx_state, header, rx) == -1) {
        return -1;
    }
    crypto_secretstream_xchacha20poly1305_init_push(&tx_state, header, tx);
    send_raw(fd, header, sizeof(header));
    return 0;
}
