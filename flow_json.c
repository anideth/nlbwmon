#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
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

int flow_json_format(const struct flow_record *record, char *buffer, size_t size)
{
    char os[INET6_ADDRSTRLEN], od[INET6_ADDRSTRLEN];
    char rs[INET6_ADDRSTRLEN], rd[INET6_ADDRSTRLEN];
    int n;

    if (!record || !buffer || !size)
        return -EINVAL;

    if (format_address(&record->original, &record->original.src, os, sizeof(os)) ||
        format_address(&record->original, &record->original.dst, od, sizeof(od)) ||
        format_address(&record->reply, &record->reply.src, rs, sizeof(rs)) ||
        format_address(&record->reply, &record->reply.dst, rd, sizeof(rd)))
        return -EINVAL;

    n = snprintf(buffer, size,
        "{\"schema\":\"nlbwmon-flow-v1\",\"id\":%" PRIu32 ",\"mark\":%" PRIu32
        ",\"family\":%u,\"protocol\":%u,\"orig_src\":\"%s\",\"orig_dst\":\"%s\""
        ",\"orig_sport\":%u,\"orig_dport\":%u,\"reply_src\":\"%s\",\"reply_dst\":\"%s\""
        ",\"reply_sport\":%u,\"reply_dport\":%u,\"orig_packets\":%" PRIu64
        ",\"orig_bytes\":%" PRIu64 ",\"reply_packets\":%" PRIu64
        ",\"reply_bytes\":%" PRIu64 ",\"started_sec\":%" PRIdMAX
        ",\"finished_sec\":%" PRIdMAX ",\"seen_new\":%s}\n",
        record->id, record->mark, (unsigned)record->original.family,
        (unsigned)record->original.protocol, os, od,
        (unsigned)record->original.sport, (unsigned)record->original.dport,
        rs, rd, (unsigned)record->reply.sport, (unsigned)record->reply.dport,
        record->orig_packets, record->orig_bytes, record->reply_packets,
        record->reply_bytes, (intmax_t)record->started.tv_sec,
        (intmax_t)record->finished.tv_sec, record->seen_new ? "true" : "false");

    if (n < 0)
        return -EIO;
    if ((size_t)n >= size)
        return -ENOSPC;
    return n;
}
