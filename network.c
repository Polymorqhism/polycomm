#include <stdlib.h>
#include <string.h>
#include "cJSON/cJSON.h"
#include <sys/socket.h>
#include <sodium.h>
#include <errno.h>

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
            if (errno == EINTR) continue;
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
        } else if (n == 0) {
            return 0;
        } else {
            if (errno == EINTR) continue;
            return -1;
        }
    }
    return total_received;
}

ssize_t send_all(int fd, const void *buffer, size_t len, crypto_secretstream_xchacha20poly1305_state *tx)
{
    unsigned long long ciphertext_len;
    unsigned char *ciphertext = malloc(len + crypto_secretstream_xchacha20poly1305_ABYTES);
    if(!ciphertext) return -1;
    crypto_secretstream_xchacha20poly1305_push(tx, ciphertext, &ciphertext_len, (const unsigned char *)buffer, len, NULL, 0, crypto_secretstream_xchacha20poly1305_TAG_MESSAGE);
    ssize_t sent = send_raw(fd, ciphertext, (size_t)ciphertext_len);
    free(ciphertext);
    return sent;
}

ssize_t recv_all(int fd, void *buffer, size_t len, crypto_secretstream_xchacha20poly1305_state *rx)
{
    unsigned long long decrypted_len;
    size_t ciphertext_len = len + crypto_secretstream_xchacha20poly1305_ABYTES;
    unsigned char *ciphertext = malloc(ciphertext_len);
    if(!ciphertext) return -1;
    ssize_t received_size = recv_raw(fd, ciphertext, ciphertext_len);
    if(received_size <= 0) {
        free(ciphertext);
        return -1;
    }
    if(crypto_secretstream_xchacha20poly1305_pull(rx, (unsigned char *)buffer, &decrypted_len, NULL, ciphertext, ciphertext_len, NULL, 0) == -1) {
        free(ciphertext);
        return -1;
    }
    free(ciphertext);
    return (ssize_t)decrypted_len;
}

int handshake_server(int fd, crypto_secretstream_xchacha20poly1305_state *tx, crypto_secretstream_xchacha20poly1305_state *rx)
{
    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
    unsigned char server_sk[crypto_kx_SECRETKEYBYTES];

    FILE *keyfile = fopen("server.key", "rb");
    if(keyfile) {
        fread(server_pk, 1, crypto_kx_PUBLICKEYBYTES, keyfile);
        fread(server_sk, 1, crypto_kx_SECRETKEYBYTES, keyfile);
        fclose(keyfile);
    } else {
        crypto_kx_keypair(server_pk, server_sk);
        keyfile = fopen("server.key", "wb");
        fwrite(server_pk, 1, crypto_kx_PUBLICKEYBYTES, keyfile);
        fwrite(server_sk, 1, crypto_kx_SECRETKEYBYTES, keyfile);
        fclose(keyfile);
    }


    send_raw(fd, server_pk, crypto_kx_PUBLICKEYBYTES);
    unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];
    recv_raw(fd, client_pk, crypto_kx_PUBLICKEYBYTES);
    unsigned char rx_key[crypto_kx_SESSIONKEYBYTES];
    unsigned char tx_key[crypto_kx_SESSIONKEYBYTES];
    if(crypto_kx_server_session_keys(rx_key, tx_key, server_pk, server_sk, client_pk) == -1) {
        return -1;
    }
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_init_push(tx, header, tx_key);
    send_raw(fd, header, sizeof(header));
    recv_raw(fd, header, sizeof(header));
    if(crypto_secretstream_xchacha20poly1305_init_pull(rx, header, rx_key) == -1) {
        return -1;
    }
    return 0;
}

int is_server_impostor(char *ip, char *server_pk_b64)
{
    int is_impostor = 1; // assume unsafe until and unless proven otherwise

    FILE *json_file = fopen("known_servers.json", "r");
    if(!json_file) {
        perror("File does not exist.");
        return 2;
    }

    fseek(json_file, 0L, SEEK_END);
    long size = ftell(json_file);
    rewind(json_file);

    char *file_buffer = (char *) malloc(size + 1);
    fread(file_buffer, 1, size, json_file);

    file_buffer[size] = '\0';

    cJSON *json = cJSON_Parse(file_buffer);
    if(!json) {
        perror("Could not parse JSON. Assuming unsafe.");
        cJSON_Delete(json);
        fclose(json_file);
        free(file_buffer);
        return is_impostor;
    }

    cJSON *server_info = cJSON_GetObjectItemCaseSensitive(json, ip);

    if(!server_info) {
        cJSON_Delete(json);
        fclose(json_file);
        free(file_buffer);
        return 2;
    }

    if(strcmp(server_info->valuestring, server_pk_b64) == 0) {
        is_impostor = 0;
    }

    fclose(json_file);
    cJSON_Delete(json);
    free(file_buffer);

    return is_impostor;
}

void mark_server_safe(char *ip, char *base64)
{
    FILE *json_file = fopen("known_servers.json", "r");
    if(!json_file) {
        json_file = fopen("known_servers.json", "w");
        if(!json_file) {
            perror("Could not create known_servers.json");
            return;
        }
        fputs("{}", json_file);
        fclose(json_file);
        json_file = fopen("known_servers.json", "r");
    }
    if(!json_file) {
        perror("File does not exist.");
        return;
    }

    fseek(json_file, 0L, SEEK_END);
    long size = ftell(json_file);
    rewind(json_file);

    char *file_buffer = (char *) malloc(size + 1);
    fread(file_buffer, 1, size, json_file);

    file_buffer[size] = '\0';

    cJSON *json = cJSON_Parse(file_buffer);
    if(!json) {
        perror("Could not parse JSON.");
        cJSON_Delete(json);
        fclose(json_file);
        free(file_buffer);
        return;
    }

    cJSON_AddStringToObject(json, ip, base64);
    char *formatted = cJSON_Print(json);
    freopen("known_servers.json", "w", json_file);
    fputs(formatted, json_file);

    fclose(json_file);
    cJSON_Delete(json);
    free(formatted);
    free(file_buffer);
}

int handshake_client(int fd, crypto_secretstream_xchacha20poly1305_state *tx, crypto_secretstream_xchacha20poly1305_state *rx, char *ip)
{
    unsigned char client_sk[crypto_kx_SECRETKEYBYTES];
    unsigned char client_pk[crypto_kx_PUBLICKEYBYTES];
    crypto_kx_keypair(client_pk, client_sk);
    unsigned char server_pk[crypto_kx_PUBLICKEYBYTES];
    recv_raw(fd, server_pk, crypto_kx_PUBLICKEYBYTES);

    char server_pk_b64[sodium_base64_ENCODED_LEN(crypto_kx_PUBLICKEYBYTES, sodium_base64_VARIANT_ORIGINAL)];
    // server anti-MiTM integrity check
    sodium_bin2base64(server_pk_b64, sizeof(server_pk_b64), server_pk, crypto_kx_PUBLICKEYBYTES, sodium_base64_VARIANT_ORIGINAL);

    int impostor = is_server_impostor(ip, server_pk_b64);
    if(impostor == 1) {
        printf("\033[1;91;107m! MiTM WARNING !\n\nYour connection is very likely being intercepted by a bad actor. The correct server public address does NOT match the one you are connecting to. To protect you, we have automatically disconnected you from the server.\n\n[%s] is the public key you just tried to connect to.\n\n\nIf this is a mistake (highly unlikely), please create an issue on the polycomm GitHub repository.\n\nBefore reconnecting, please ensure your network is safe.\033[0m \n", server_pk_b64);

        return -1;
    } else if(impostor == 2) {
        printf("This is the first time you are connecting to this server with a server public key [%s].\n\nIf you're sure the connection is safe, continue by pressing ENTER.\n\nOtherwise, click CTRL+C to exit out and cancel.\n", server_pk_b64);
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        mark_server_safe(ip, server_pk_b64);
    }
    // end
    send_raw(fd, client_pk, crypto_kx_PUBLICKEYBYTES);
    unsigned char rx_key[crypto_kx_SESSIONKEYBYTES];
    unsigned char tx_key[crypto_kx_SESSIONKEYBYTES];
    if(crypto_kx_client_session_keys(rx_key, tx_key, client_pk, client_sk, server_pk) == -1) {
        return -1;
    }
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    recv_raw(fd, header, sizeof(header));
    if(crypto_secretstream_xchacha20poly1305_init_pull(rx, header, rx_key) == -1) {
        return -1;
    }
    crypto_secretstream_xchacha20poly1305_init_push(tx, header, tx_key);
    send_raw(fd, header, sizeof(header));
    return 0;
}
