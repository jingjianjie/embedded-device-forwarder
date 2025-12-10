#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../routerd/include/event.h"

#define SOCKET_PATH "/tmp/ez_router.sock"

int main()
{
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to ez_routerd!\n");

    const char* msg = "hello routerd";
    send(fd, msg, strlen(msg), 0);

    event_msg_t ev;
    int n = recv(fd, &ev, sizeof(ev), 0);

    printf("Received %d bytes: type=%d, len=%d, data=%.*s\n",
           n, ev.type, ev.len, ev.len, ev.data);

    close(fd);
    return 0;
}
