#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "flow_tcp.h"

int main(void)
{
    struct flow_tcp_config bad = { .host = "127.0.0.1", .port = 5515,
                                   .queue_size = 0, .reconnect_interval = 5 };
    struct flow_tcp_config good = { .host = "127.0.0.1", .port = 5515,
                                    .queue_size = 2, .reconnect_interval = 5 };
    struct flow_tcp *tcp;

    assert(flow_tcp_create(&bad) == NULL);
    tcp = flow_tcp_create(&good);
    assert(tcp);
    assert(flow_tcp_queued(tcp) == 0);
    assert(flow_tcp_enqueue(tcp, "a\n", 2) == 0);
    assert(flow_tcp_enqueue(tcp, "b\n", 2) == 0);
    assert(flow_tcp_queued(tcp) == 2);
    assert(flow_tcp_enqueue(tcp, "c\n", 2) == -ENOSPC);
    assert(flow_tcp_dropped(tcp) == 1);
    assert(flow_tcp_enqueue(tcp, "bad", 3) == -EINVAL);
    flow_tcp_destroy(tcp);
    puts("flow TCP queue tests: PASS");
    return 0;
}
