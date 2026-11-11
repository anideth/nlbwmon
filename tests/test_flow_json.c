#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "flow_json.h"

int main(void)
{
    struct flow_record r = { 0 };
    char json[1024];
    char tiny[8];
    int n;

    r.id = 42;
    r.mark = 0x40000000U;
    r.original.family = AF_INET;
    r.reply.family = AF_INET;
    r.original.protocol = IPPROTO_TCP;
    r.reply.protocol = IPPROTO_TCP;
    r.original.sport = htons(12345);
    r.original.dport = htons(443);
    r.reply.sport = htons(443);
    r.reply.dport = htons(12345);
    r.orig_bytes = 1234567890123ULL;
    r.reply_bytes = 999;
    r.started.tv_sec = 100;
    r.finished.tv_sec = 200;
    r.seen_new = true;
    assert(inet_pton(AF_INET, "192.0.2.10", &r.original.src.s6_addr[0]) == 1);
    assert(inet_pton(AF_INET, "198.51.100.20", &r.original.dst.s6_addr[0]) == 1);
    r.reply.src = r.original.dst;
    r.reply.dst = r.original.src;

    n = flow_json_format(&r, json, sizeof(json));
    assert(n > 0);
    assert((size_t)n == strlen(json));
    assert(strstr(json, "\"orig_src\":\"192.0.2.10\""));
    assert(strstr(json, "\"orig_bytes\":1234567890123"));
    assert(strstr(json, "\"orig_sport\":12345"));
    assert(strstr(json, "\"orig_dport\":443"));
    assert(strstr(json, "\"reply_sport\":443"));
    assert(strstr(json, "\"reply_dport\":12345"));
    assert(strstr(json, "\"seen_new\":true"));
    assert(json[n - 1] == '\n');
    assert(flow_json_format(&r, tiny, sizeof(tiny)) == -ENOSPC);

    r.original.family = AF_INET6;
    r.reply.family = AF_INET6;
    assert(inet_pton(AF_INET6, "2001:db8::1", &r.original.src) == 1);
    assert(inet_pton(AF_INET6, "2001:db8::2", &r.original.dst) == 1);
    r.reply.src = r.original.dst;
    r.reply.dst = r.original.src;
    assert(flow_json_format(&r, json, sizeof(json)) > 0);
    assert(strstr(json, "\"orig_src\":\"2001:db8::1\""));

    puts("flow JSON tests: PASS");
    return 0;
}
