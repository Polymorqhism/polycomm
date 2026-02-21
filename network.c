#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>

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
