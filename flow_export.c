#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flow_export.h"

#define FLOW_MARK_MASK 0x40000000U
#define FLOW_BUCKETS 4096
#define FLOW_MAX_ACTIVE 16384

struct flow_entry {
    struct flow_record record;
    struct flow_entry *next;
};

static struct flow_entry *flows[FLOW_BUCKETS];
static size_t active_count;

size_t flow_active_count(void)
{
    return active_count;
}
static flow_complete_cb complete_callback;
static void *complete_context;

void flow_set_complete_callback(flow_complete_cb callback, void *ctx)
{
    complete_callback = callback;
    complete_context = ctx;
}

static unsigned int flow_hash(uint32_t id)
{
    return (id ^ (id >> 16)) % FLOW_BUCKETS;
}

static bool tuple_equal(const struct flow_tuple *a,
                        const struct flow_tuple *b)
{
    return a->family == b->family &&
           a->protocol == b->protocol &&
           a->sport == b->sport &&
           a->dport == b->dport &&
           memcmp(&a->src, &b->src, sizeof(a->src)) == 0 &&
           memcmp(&a->dst, &b->dst, sizeof(a->dst)) == 0;
}

static bool flow_equal(const struct flow_record *a,
                       const struct flow_record *b)
{
    return a->id == b->id &&
           tuple_equal(&a->original, &b->original) &&
           tuple_equal(&a->reply, &b->reply);
}

static struct flow_entry **flow_find(const struct flow_record *record)
{
    struct flow_entry **entry = &flows[flow_hash(record->id)];

    while (*entry) {
        if (flow_equal(&(*entry)->record, record))
            break;
        entry = &(*entry)->next;
    }

    return entry;
}

int flow_init(void)
{
    flow_cleanup();
    return 0;
}

void flow_cleanup(void)
{
    unsigned int i;

    for (i = 0; i < FLOW_BUCKETS; i++) {
        struct flow_entry *entry = flows[i];

        while (entry) {
            struct flow_entry *next = entry->next;
            free(entry);
            entry = next;
        }

        flows[i] = NULL;
    }
    active_count = 0;
}

int flow_process(enum flow_event_type event,
                 const struct flow_record *record)
{
    struct flow_entry **slot;
    struct flow_entry *entry;
    struct flow_record *stored;

    if (!record)
        return -EINVAL;

    if (!(record->mark & FLOW_MARK_MASK))
        return 0;

    if (event != FLOW_NEW &&
        event != FLOW_UPDATE &&
        event != FLOW_DESTROY)
        return -EINVAL;

    slot = flow_find(record);
    entry = *slot;

    if (!entry) {
        if (active_count >= FLOW_MAX_ACTIVE)
            return -ENOSPC;

        entry = calloc(1, sizeof(*entry));
        if (!entry)
            return -ENOMEM;

        entry->record = *record;

        entry->record.orig_packets = 0;
        entry->record.orig_bytes = 0;
        entry->record.reply_packets = 0;
        entry->record.reply_bytes = 0;

        entry->record.seen_new = (event == FLOW_NEW);

        if (entry->record.started.tv_sec == 0 &&
            entry->record.started.tv_nsec == 0)
            clock_gettime(CLOCK_REALTIME, &entry->record.started);

        *slot = entry;
        active_count++;
    }

    stored = &entry->record;

    if (event == FLOW_NEW) {
        if (!stored->seen_new) {
            stored->seen_new = true;
            /* Preserve the earliest observed start time. */
        }
    } else {
        stored->orig_packets += record->orig_packets;
        stored->orig_bytes += record->orig_bytes;
        stored->reply_packets += record->reply_packets;
        stored->reply_bytes += record->reply_bytes;
    }

    if (event == FLOW_DESTROY) {
        stored->finished = record->finished;

        if (stored->finished.tv_sec == 0 &&
            stored->finished.tv_nsec == 0)
            clock_gettime(CLOCK_REALTIME, &stored->finished);

        if (complete_callback)
            complete_callback(stored, complete_context);

        *slot = entry->next;
        free(entry);
        active_count--;
    }

    return 0;
}
