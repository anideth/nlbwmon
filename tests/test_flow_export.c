#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "../flow_export.h"

static struct flow_record completed;
static unsigned int completions;

static void on_complete(const struct flow_record *r, void *ctx)
{
    (void)ctx;
    completed = *r;
    completions++;
}

static struct flow_record sample(uint32_t id, uint32_t mark)
{
    struct flow_record r = { 0 };
    r.id = id;
    r.mark = mark;
    r.original.family = AF_INET;
    r.reply.family = AF_INET;
    r.original.protocol = IPPROTO_TCP;
    r.reply.protocol = IPPROTO_TCP;
    r.original.sport = htons(50000);
    r.original.dport = htons(443);
    r.reply.sport = htons(443);
    r.reply.dport = htons(50000);
    inet_pton(AF_INET, "192.0.2.10", &r.original.src);
    inet_pton(AF_INET, "198.51.100.20", &r.original.dst);
    inet_pton(AF_INET, "198.51.100.20", &r.reply.src);
    inet_pton(AF_INET, "192.0.2.10", &r.reply.dst);
    return r;
}

static void reset(void)
{
    flow_cleanup();
    memset(&completed, 0, sizeof(completed));
    completions = 0;
}

static void test_lifecycle(void)
{
    struct flow_record r = sample(1, 0x40000000U);
    reset();
    assert(flow_process(FLOW_NEW, &r) == 0);
    r.orig_bytes = 100;
    r.orig_packets = 2;
    r.reply_bytes = 200;
    r.reply_packets = 3;
    assert(flow_process(FLOW_UPDATE, &r) == 0);
    r.orig_bytes = 50;
    r.orig_packets = 1;
    r.reply_bytes = 70;
    r.reply_packets = 1;
    assert(flow_process(FLOW_UPDATE, &r) == 0);
    r.orig_bytes = 10;
    r.orig_packets = 1;
    r.reply_bytes = 20;
    r.reply_packets = 1;
    assert(flow_process(FLOW_DESTROY, &r) == 0);
    assert(completions == 1);
    assert(completed.orig_bytes == 160);
    assert(completed.reply_bytes == 290);
    assert(completed.orig_packets == 4);
    assert(completed.reply_packets == 5);
    assert(completed.seen_new);
    assert(completed.started.tv_sec > 0);
    assert(completed.finished.tv_sec > 0);
}

static void test_unmarked(void)
{
    struct flow_record r = sample(2, 0);
    reset();
    assert(flow_process(FLOW_NEW, &r) == 0);
    assert(flow_process(FLOW_DESTROY, &r) == 0);
    assert(completions == 0);
}

static void test_missing_new(void)
{
    struct flow_record r = sample(3, 0x40000000U);
    reset();
    r.orig_bytes = 100;
    assert(flow_process(FLOW_UPDATE, &r) == 0);
    r.orig_bytes = 25;
    assert(flow_process(FLOW_DESTROY, &r) == 0);
    assert(completions == 1);
    assert(completed.orig_bytes == 125);
    assert(!completed.seen_new);
}

static void test_interleaved_ids(void)
{
    struct flow_record a = sample(4, 0x40000000U);
    struct flow_record b = sample(5, 0x40000000U);
    reset();
    a.orig_bytes = 11;
    b.orig_bytes = 22;
    assert(flow_process(FLOW_UPDATE, &a) == 0);
    assert(flow_process(FLOW_UPDATE, &b) == 0);
    a.orig_bytes = 3;
    assert(flow_process(FLOW_DESTROY, &a) == 0);
    assert(completed.id == 4 && completed.orig_bytes == 14);
    b.orig_bytes = 4;
    assert(flow_process(FLOW_DESTROY, &b) == 0);
    assert(completed.id == 5 && completed.orig_bytes == 26);
    assert(completions == 2);
}

int main(void)
{
    flow_set_complete_callback(on_complete, NULL);
    test_lifecycle();
    test_unmarked();
    test_missing_new();
    test_interleaved_ids();
    flow_cleanup();
    puts("flow accounting tests: PASS");
    return 0;
}
