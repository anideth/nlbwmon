#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "flow_tcp.h"

static void pump_until_empty(struct flow_tcp *tcp)
{
    unsigned int i;
    for (i = 0; i < 400 && flow_tcp_queued(tcp); i++) {
        flow_tcp_step(tcp);
        usleep(10000);
    }
    assert(flow_tcp_queued(tcp) == 0);
}

static void read_line(int fd, const char *expected)
{
    char buf[128];
    size_t received = 0, length = strlen(expected);
    while (received < length) {
        ssize_t n = recv(fd, buf + received, length - received, 0);
        assert(n > 0);
        received += (size_t)n;
    }
    assert(memcmp(buf, expected, length) == 0);
}

int main(void)
{
    int server, peer;
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = 0 };
    socklen_t len = sizeof(addr);
    struct linger reset = { .l_onoff = 1, .l_linger = 0 };
    struct flow_tcp_config cfg = { .host = "127.0.0.1", .queue_size = 4,
                                   .reconnect_interval = 1 };
    struct flow_tcp *tcp;
    const char *first = "{\"sequence\":1}\n";
    const char *second = "{\"sequence\":2}\n";

    server = socket(AF_INET, SOCK_STREAM, 0);
    assert(server >= 0);
    assert(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    assert(bind(server, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(server, 2) == 0);
    assert(getsockname(server, (struct sockaddr *)&addr, &len) == 0);
    cfg.port = ntohs(addr.sin_port);

    tcp = flow_tcp_create(&cfg);
    assert(tcp);
    assert(flow_tcp_enqueue(tcp, first, strlen(first)) == 0);
    pump_until_empty(tcp);
    peer = accept(server, NULL, NULL);
    assert(peer >= 0);
    read_line(peer, first);

    /* RST the established connection, then queue a new line. */
    assert(setsockopt(peer, SOL_SOCKET, SO_LINGER, &reset, sizeof(reset)) == 0);
    close(peer);
    usleep(50000);
    assert(flow_tcp_enqueue(tcp, second, strlen(second)) == 0);
    pump_until_empty(tcp);

    peer = accept(server, NULL, NULL);
    assert(peer >= 0);
    read_line(peer, second);
    assert(flow_tcp_dropped(tcp) == 0);
    close(peer);
    close(server);
    flow_tcp_destroy(tcp);
    puts("flow TCP reset/reconnect tests: PASS");
    return 0;
}
