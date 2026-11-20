#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "../flow_export.h"

static struct flow_record completed;
static unsigned int completions;
static unsigned int starts;
static struct flow_record started;

static void on_complete(const struct flow_record *r, void *ctx)
{
    (void)ctx;
    completed = *r;
    completions++;
}

static void on_start(const struct flow_record *r, void *ctx)
{
    (void)ctx;
    started = *r;
    starts++;
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
    starts = 0;
    memset(&started, 0, sizeof(started));
    assert(flow_active_count() == 0);
}

static void test_lifecycle(void)
{
    struct flow_record r = sample(1, 0x40000000U);
    reset();
    assert(flow_process(FLOW_NEW, &r) == 0);
    assert(flow_process(FLOW_NEW, &r) == 0);
    assert(flow_active_count() == 1);
    assert(starts == 1);
    assert(started.id == r.id);
    assert(started.started.tv_sec > 0);
    assert(started.orig_bytes == 0);
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
    assert(starts == 1);
    assert(completed.started.tv_sec == started.started.tv_sec);
    assert(completed.started.tv_nsec == started.started.tv_nsec);
    assert(completed.orig_bytes == 160);
    assert(completed.reply_bytes == 290);
    assert(completed.orig_packets == 4);
    assert(completed.reply_packets == 5);
    assert(flow_active_count() == 0);
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
    assert(starts == 0);
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
    assert(flow_active_count() == 0);
}

static void test_active_limit(void)
{
    struct flow_record r;
    unsigned int i;
    reset();
    for (i = 0; i < 16384; i++) {
        r = sample(1000 + i, 0x40000000U);
        assert(flow_process(FLOW_NEW, &r) == 0);
    }
    assert(flow_active_count() == 16384);
    r = sample(99999, 0x40000000U);
    assert(flow_process(FLOW_NEW, &r) == -ENOSPC);
    assert(flow_active_count() == 16384);
    r = sample(1000, 0x40000000U);
    assert(flow_process(FLOW_DESTROY, &r) == 0);
    assert(flow_active_count() == 16383);
    r = sample(99999, 0x40000000U);
    assert(flow_process(FLOW_NEW, &r) == 0);
    assert(flow_active_count() == 16384);
    reset();
}

static void test_disabled_and_config(void)
{
    struct flow_config cfg = { .enabled = false, .mark = 0x40000000U, .mark_mask = 0x40000000U, .max_flows = 2 };
    struct flow_record r = sample(77, 0x40000000U);
    reset();
    assert(flow_configure(&cfg) == 0);
    assert(flow_process(FLOW_NEW, &r) == 0);
    assert(flow_active_count() == 0);
    cfg.enabled = true;
    cfg.mark = 0x10000000U;
    cfg.mark_mask = 0x30000000U;
    assert(flow_configure(&cfg) == 0);
    assert(flow_process(FLOW_NEW, &r) == 0);
    assert(flow_active_count() == 0);
    r.mark = 0x50000000U;
    assert(flow_process(FLOW_NEW, &r) == 0);
    assert(flow_active_count() == 1);
    assert(flow_configure(&cfg) == -EBUSY);
    r.id++;
    assert(flow_process(FLOW_NEW, &r) == 0);
    r.id++;
    assert(flow_process(FLOW_NEW, &r) == -ENOSPC);
    reset();
    cfg.mark = 0x40000000U;
    assert(flow_configure(&cfg) == -EINVAL);
}

static void test_multiple_zero_dumps_and_residual(void)
{
    struct flow_record r = sample(600, 0x40000000U);
    reset();
    assert(flow_process(FLOW_NEW, &r) == 0);
    r.orig_bytes = 1000; r.reply_bytes = 2000;
    r.orig_packets = 10; r.reply_packets = 20;
    assert(flow_process(FLOW_UPDATE, &r) == 0);
    /* GET_CTRZERO zeroed counters: no traffic between polls. */
    r.orig_bytes = 0; r.reply_bytes = 0;
    r.orig_packets = 0; r.reply_packets = 0;
    assert(flow_process(FLOW_UPDATE, &r) == 0);
    r.orig_bytes = 300; r.reply_bytes = 400;
    r.orig_packets = 3; r.reply_packets = 4;
    assert(flow_process(FLOW_UPDATE, &r) == 0);
    /* DESTROY returns only traffic since the last counter-zero dump. */
    r.orig_bytes = 7; r.reply_bytes = 9;
    r.orig_packets = 1; r.reply_packets = 1;
    assert(flow_process(FLOW_DESTROY, &r) == 0);
    assert(completions == 1);
    assert(completed.orig_bytes == 1307);
    assert(completed.reply_bytes == 2409);
    assert(completed.orig_packets == 14);
    assert(completed.reply_packets == 25);
    assert(flow_active_count() == 0);
}

static void test_destroy_without_prior_events(void)
{
    struct flow_record r = sample(601, 0x40000000U);
    reset();
    r.orig_bytes = 123; r.reply_bytes = 456;
    assert(flow_process(FLOW_DESTROY, &r) == 0);
    assert(completions == 1);
    assert(completed.orig_bytes == 123);
    assert(completed.reply_bytes == 456);
    assert(!completed.seen_new);
    assert(flow_active_count() == 0);
}

int main(void)
{
    struct flow_config cfg = { .enabled = true, .mark = 0x40000000U, .mark_mask = 0x40000000U, .max_flows = 16384 };
    assert(flow_configure(&cfg) == 0);
    flow_set_start_callback(on_start, NULL);
    flow_set_complete_callback(on_complete, NULL);
    test_lifecycle();
    test_multiple_zero_dumps_and_residual();
    test_destroy_without_prior_events();
    test_unmarked();
    test_missing_new();
    test_interleaved_ids();
    test_active_limit();
    test_disabled_and_config();
    flow_cleanup();
    puts("flow accounting tests: PASS");
    return 0;
}
