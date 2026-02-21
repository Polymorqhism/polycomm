#include <sys/types.h>


ssize_t send_all(int fd, const void *buffer, size_t len);
ssize_t recv_all(int fd, void *buffer, size_t len);
