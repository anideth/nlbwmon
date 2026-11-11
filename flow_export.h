#ifndef FLOW_EXPORT_H
#define FLOW_EXPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>

struct flow_tuple {
    uint8_t family;
    uint8_t protocol;
    struct in6_addr src;
    struct in6_addr dst;
    uint16_t sport;
    uint16_t dport;
};

struct flow_record {
    uint32_t id;
    uint32_t mark;

    struct flow_tuple original;
    struct flow_tuple reply;

    uint64_t orig_packets;
    uint64_t orig_bytes;
    uint64_t reply_packets;
    uint64_t reply_bytes;

    struct timespec started;
    struct timespec finished;

    bool seen_new;
};

enum flow_event_type {
    FLOW_NEW,
    FLOW_UPDATE,
    FLOW_DESTROY
};

typedef void (*flow_start_cb)(const struct flow_record *record, void *ctx);
typedef void (*flow_complete_cb)(const struct flow_record *record, void *ctx);

struct flow_config {
    bool enabled;
    uint32_t mark;
    uint32_t mark_mask;
    size_t max_flows;
};

int flow_configure(const struct flow_config *config);
void flow_set_start_callback(flow_start_cb callback, void *ctx);
void flow_set_complete_callback(flow_complete_cb callback, void *ctx);
size_t flow_active_count(void);
int flow_init(void);
void flow_cleanup(void);

int flow_process(enum flow_event_type event,
                 const struct flow_record *record);

#endif
