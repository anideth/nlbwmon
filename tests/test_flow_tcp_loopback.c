#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "flow_tcp.h"

int main(void)
{
    int server = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = { .sin_family = AF_INET, .sin_port = 0 };
    socklen_t length = sizeof(address);
    struct flow_tcp_config config = { .host = "127.0.0.1", .queue_size = 8,
                                      .reconnect_interval = 1 };
    struct flow_tcp *tcp;
    int peer = -1;
    char received[128] = { 0 };
    ssize_t count;
    unsigned int i;
    const char *line = "{\"test\":true}\n";

    assert(server >= 0);
    assert(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);
    assert(bind(server, (struct sockaddr *)&address, sizeof(address)) == 0);
    assert(getsockname(server, (struct sockaddr *)&address, &length) == 0);
    assert(listen(server, 1) == 0);
    config.port = ntohs(address.sin_port);
    tcp = flow_tcp_create(&config);
    assert(tcp);
    assert(flow_tcp_enqueue(tcp, line, strlen(line)) == 0);

    /* The client is polled like it would be from uloop. */
    for (i = 0; i < 200 && flow_tcp_queued(tcp); i++) {
        flow_tcp_step(tcp);
        usleep(10000);
    }
    assert(flow_tcp_queued(tcp) == 0);
    peer = accept(server, NULL, NULL);
    assert(peer >= 0);
    count = recv(peer, received, sizeof(received), 0);
    assert(count == (ssize_t)strlen(line));
    assert(memcmp(received, line, (size_t)count) == 0);
    assert(flow_tcp_dropped(tcp) == 0);
    close(peer);
    close(server);
    flow_tcp_destroy(tcp);
    puts("flow TCP loopback tests: PASS");
    return 0;
}
