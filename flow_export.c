#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flow_export.h"

#define FLOW_MARK_MASK 0x40000000U
#define FLOW_BUCKETS 4096

struct flow_entry {
    struct flow_record record;
    struct flow_entry *next;
};

static struct flow_entry *flows[FLOW_BUCKETS];

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

    slot = flow_find(record);
    entry = *slot;

    if (!entry) {
        entry = calloc(1, sizeof(*entry));

        if (!entry)
            return -ENOMEM;

        entry->record = *record;
        entry->record.orig_packets = 0;
        entry->record.orig_bytes = 0;
        entry->record.reply_packets = 0;
        entry->record.reply_bytes = 0;
        entry->record.seen_new = (event == FLOW_NEW);

        *slot = entry;
    }

    stored = &entry->record;

    if (event == FLOW_NEW) {
        if (!stored->seen_new) {
            stored->started = record->started;
            stored->seen_new = true;
        }
    } else {
        stored->orig_packets += record->orig_packets;
        stored->orig_bytes += record->orig_bytes;
        stored->reply_packets += record->reply_packets;
        stored->reply_bytes += record->reply_bytes;
    }

    if (event == FLOW_DESTROY) {
        stored->finished = record->finished;

        /* TODO: export completed flow before removing it */
        fprintf(stderr,
                "flow completed: id=%u sent=%llu received=%llu\n",
                stored->id,
                (unsigned long long)stored->orig_bytes,
                (unsigned long long)stored->reply_bytes);

        *slot = entry->next;
        free(entry);
    }

    return 0;
}
