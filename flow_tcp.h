#ifndef FLOW_TCP_H
#define FLOW_TCP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct flow_tcp;

struct flow_tcp_config {
    const char *host;
    uint16_t port;
    size_t queue_size;
    unsigned int reconnect_interval;
};

/* Nonblocking transport. Call flow_tcp_step periodically from the event loop. */
struct flow_tcp *flow_tcp_create(const struct flow_tcp_config *config);
void flow_tcp_destroy(struct flow_tcp *transport);
int flow_tcp_enqueue(struct flow_tcp *transport, const char *json, size_t length);
void flow_tcp_step(struct flow_tcp *transport);
size_t flow_tcp_queued(const struct flow_tcp *transport);
uint64_t flow_tcp_dropped(const struct flow_tcp *transport);

#endif
