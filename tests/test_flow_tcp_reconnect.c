#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "flow_tcp.h"

static int listener(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port) };
    assert(fd >= 0);
    assert(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0);
    assert(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    assert(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(fd, 1) == 0);
    return fd;
}

int main(void)
{
    int server, peer;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    struct flow_tcp_config cfg = { .host = "127.0.0.1", .queue_size = 2,
                                   .reconnect_interval = 1 };
    struct flow_tcp *tcp;
    char buffer[128] = { 0 };
    const char *line = "{\"reconnect\":true}\n";
    unsigned int i;
    ssize_t n;

    server = listener(0);
    assert(getsockname(server, (struct sockaddr *)&addr, &addrlen) == 0);
    cfg.port = ntohs(addr.sin_port);
    close(server); /* Reserve the chosen port, then simulate an offline receiver. */

    tcp = flow_tcp_create(&cfg);
    assert(tcp);
    assert(flow_tcp_enqueue(tcp, line, strlen(line)) == 0);
    for (i = 0; i < 30; i++) {
        flow_tcp_step(tcp);
        usleep(10000);
    }
    assert(flow_tcp_queued(tcp) == 1);
    assert(flow_tcp_dropped(tcp) == 0);

    server = listener(cfg.port);
    for (i = 0; i < 400 && flow_tcp_queued(tcp); i++) {
        flow_tcp_step(tcp);
        usleep(10000);
    }
    assert(flow_tcp_queued(tcp) == 0);
    peer = accept(server, NULL, NULL);
    assert(peer >= 0);
    n = recv(peer, buffer, sizeof(buffer), 0);
    assert(n == (ssize_t)strlen(line));
    assert(memcmp(buffer, line, (size_t)n) == 0);
    assert(flow_tcp_dropped(tcp) == 0);

    close(peer);
    close(server);
    flow_tcp_destroy(tcp);
    puts("flow TCP reconnect tests: PASS");
    return 0;
}
