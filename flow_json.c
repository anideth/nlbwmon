#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "flow_json.h"

static int format_address(const struct flow_tuple *tuple, const struct in6_addr *addr,
                          char *output, size_t size)
{
    const void *src = addr;

    if (tuple->family == AF_INET)
        src = &addr->s6_addr[0];
    else if (tuple->family != AF_INET6)
        return -EAFNOSUPPORT;

    return inet_ntop(tuple->family, src, output, size) ? 0 : -EINVAL;
}

int flow_json_format_event(const struct flow_record *record, const char *event_type,
                           const char *boot_id, char *buffer, size_t size)
{
    char os[INET6_ADDRSTRLEN], od[INET6_ADDRSTRLEN];
    char rs[INET6_ADDRSTRLEN], rd[INET6_ADDRSTRLEN];
    int n;

    if (!record || !buffer || !size || !event_type || !boot_id ||
        (strcmp(event_type, "NEW") && strcmp(event_type, "DESTROY")))
        return -EINVAL;

    if (format_address(&record->original, &record->original.src, os, sizeof(os)) ||
        format_address(&record->original, &record->original.dst, od, sizeof(od)) ||
        format_address(&record->reply, &record->reply.src, rs, sizeof(rs)) ||
        format_address(&record->reply, &record->reply.dst, rd, sizeof(rd)))
        return -EINVAL;

    n = snprintf(buffer, size,
        "{\"schema\":\"nlbwmon-flow-v1\",\"event_type\":\"%s\",\"connection_id\":\"%s-%" PRIu32 "-%" PRIdMAX "-%ld\",\"id\":%" PRIu32 ",\"mark\":%" PRIu32
        ",\"family\":%u,\"protocol\":%u,\"orig_src\":\"%s\",\"orig_dst\":\"%s\""
        ",\"orig_sport\":%u,\"orig_dport\":%u,\"reply_src\":\"%s\",\"reply_dst\":\"%s\""
        ",\"reply_sport\":%u,\"reply_dport\":%u,\"orig_packets\":%" PRIu64
        ",\"orig_bytes\":%" PRIu64 ",\"reply_packets\":%" PRIu64
        ",\"reply_bytes\":%" PRIu64 ",\"started_sec\":%" PRIdMAX
        ",\"finished_sec\":%" PRIdMAX ",\"seen_new\":%s}\n",
        event_type, boot_id, record->id, (intmax_t)record->started.tv_sec,
        record->started.tv_nsec, record->id, record->mark, (unsigned)record->original.family,
        (unsigned)record->original.protocol, os, od,
        (unsigned)ntohs(record->original.sport), (unsigned)ntohs(record->original.dport),
        rs, rd, (unsigned)ntohs(record->reply.sport), (unsigned)ntohs(record->reply.dport),
        record->orig_packets, record->orig_bytes, record->reply_packets,
        record->reply_bytes, (intmax_t)record->started.tv_sec,
        (intmax_t)record->finished.tv_sec, record->seen_new ? "true" : "false");

    if (n < 0)
        return -EIO;
    if ((size_t)n >= size)
        return -ENOSPC;
    return n;
}

int flow_json_format(const struct flow_record *record, char *buffer, size_t size)
{
    return flow_json_format_event(record, "DESTROY", "legacy", buffer, size);
}
