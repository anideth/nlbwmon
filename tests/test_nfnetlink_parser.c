/*
 * Integration test: synthetic NFNETLINK conntrack messages pass through
 * nfnetlink_parse_event() (the production parser) into flow_process().
 * Link nfnetlink.c with -ffunction-sections -Wl,--gc-sections to discard
 * unrelated daemon/socket/database entry points.
 */
#include <assert.h>
#include <arpa/inet.h>
#include <endian.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../nfnetlink.h"
#include "../subnets.h"
#include "../neigh.h"
#include "../protocol.h"
#include "../flow_export.h"

/* Stubs for legacy accounting; flow parsing/tracking remain production code. */
struct dbhandle *gdbh;
struct options opt;

int database_insert(struct dbhandle *h, struct record *rec)
{
    (void)h; (void)rec;
    return 0;
}

int database_update(struct dbhandle *h, struct record *rec)
{
    (void)h; (void)rec;
    return 0;
}

int update_macaddr(int family, const void *addr)
{
    (void)family; (void)addr;
    return 0;
}

int lookup_macaddr(int family, const void *addr, struct ether_addr *mac)
{
    (void)family; (void)addr; (void)mac;
    return 0;
}

struct protocol *lookup_protocol(uint8_t proto, uint16_t port)
{
    (void)proto; (void)port;
    return NULL;
}

/* No test traffic is in the legacy nlbwmon accounting subnets. */
int match_subnet(int family, struct in6_addr *addr)
{
    (void)family;
    (void)addr;
    return 0;
}

static struct flow_record completed;
static unsigned int completions;

static void complete(const struct flow_record *record, void *ctx)
{
    (void)ctx;
    completed = *record;
    completions++;
}

static void tuple(struct nl_msg *msg, int kind, const char *src,
                  const char *dst, uint16_t sport, uint16_t dport)
{
    struct nlattr *outer, *ip, *proto;
    struct in_addr a, b;
    assert(inet_pton(AF_INET, src, &a) == 1);
    assert(inet_pton(AF_INET, dst, &b) == 1);
    outer = nla_nest_start(msg, kind);
    assert(outer);
    ip = nla_nest_start(msg, CTA_TUPLE_IP);
    assert(ip);
    assert(nla_put(msg, CTA_IP_V4_SRC, sizeof(a), &a) == 0);
    assert(nla_put(msg, CTA_IP_V4_DST, sizeof(b), &b) == 0);
    nla_nest_end(msg, ip);
    proto = nla_nest_start(msg, CTA_TUPLE_PROTO);
    assert(proto);
    assert(nla_put_u8(msg, CTA_PROTO_NUM, IPPROTO_TCP) == 0);
    assert(nla_put_u16(msg, CTA_PROTO_SRC_PORT, htons(sport)) == 0);
    assert(nla_put_u16(msg, CTA_PROTO_DST_PORT, htons(dport)) == 0);
    nla_nest_end(msg, proto);
    nla_nest_end(msg, outer);
}

static void counters(struct nl_msg *msg, int kind, uint64_t packets,
                     uint64_t bytes, bool include_bytes)
{
    struct nlattr *nest = nla_nest_start(msg, kind);
    assert(nest);
    assert(nla_put_u64(msg, CTA_COUNTERS_PACKETS, htobe64(packets)) == 0);
    if (include_bytes)
        assert(nla_put_u64(msg, CTA_COUNTERS_BYTES, htobe64(bytes)) == 0);
    nla_nest_end(msg, nest);
}

static void emit(unsigned int type, bool dump, bool include_mark,
                 bool include_bytes, uint64_t op, uint64_t ob,
                 uint64_t rp, uint64_t rb)
{
    struct nl_msg *msg;
    struct nfgenmsg nf = { .nfgen_family = AF_INET,
                           .version = NFNETLINK_V0 };
    uint32_t id = htonl(0x12345678U);
    uint32_t mark = htonl(0x40000000U);
    msg = nlmsg_alloc_simple((NFNL_SUBSYS_CTNETLINK << 8) | type, 0);
    assert(msg);
    assert(nlmsg_append(msg, &nf, sizeof(nf), NLMSG_ALIGNTO) == 0);
    tuple(msg, CTA_TUPLE_ORIG, "192.0.2.10", "198.51.100.20", 51000, 443);
    /* NAT: reply destination differs from original source. */
    tuple(msg, CTA_TUPLE_REPLY, "198.51.100.20", "203.0.113.7", 443, 61000);
    assert(nla_put(msg, CTA_ID, sizeof(id), &id) == 0);
    if (include_mark)
        assert(nla_put(msg, CTA_MARK, sizeof(mark), &mark) == 0);
    if (type != IPCTNL_MSG_CT_NEW || dump) {
        counters(msg, CTA_COUNTERS_ORIG, op, ob, include_bytes);
        counters(msg, CTA_COUNTERS_REPLY, rp, rb, include_bytes);
    }
    nfnetlink_parse_event(nlmsg_hdr(msg), nlmsg_hdr(msg)->nlmsg_len,
                          false, false, dump);
    nlmsg_free(msg);
}

int main(void)
{
    struct flow_config cfg = { .enabled = true, .mark = 0x40000000U,
                               .mark_mask = 0x40000000U, .max_flows = 32 };
    struct in_addr expected;
    assert(flow_configure(&cfg) == 0);
    flow_set_complete_callback(complete, NULL);

    emit(IPCTNL_MSG_CT_NEW, false, true, true, 0, 0, 0, 0);
    assert(flow_active_count() == 1);
    emit(IPCTNL_MSG_CT_GET_CTRZERO, true, true, true, 2, 100, 3, 200);
    emit(IPCTNL_MSG_CT_GET_CTRZERO, true, true, true, 1, 50, 1, 70);
    /* Missing optional bytes must not reuse previous nested attributes. */
    emit(IPCTNL_MSG_CT_GET_CTRZERO, true, true, false, 4, 999, 5, 999);
    /* Missing mark is ignored rather than corrupting an active flow. */
    emit(IPCTNL_MSG_CT_DELETE, false, false, true, 1, 1, 1, 1);
    assert(flow_active_count() == 1);
    assert(completions == 0);
    emit(IPCTNL_MSG_CT_DELETE, false, true, true, 1, 10, 2, 20);
    assert(completions == 1);
    assert(flow_active_count() == 0);
    assert(completed.id == 0x12345678U);
    assert(completed.mark == 0x40000000U);
    assert(completed.seen_new);
    assert(completed.orig_packets == 8);
    assert(completed.orig_bytes == 160);
    assert(completed.reply_packets == 11);
    assert(completed.reply_bytes == 290);
    assert(completed.original.sport == htons(51000));
    assert(completed.original.dport == htons(443));
    assert(completed.reply.sport == htons(443));
    assert(completed.reply.dport == htons(61000));
    assert(inet_pton(AF_INET, "203.0.113.7", &expected) == 1);
    assert(memcmp(&completed.reply.dst, &expected, sizeof(expected)) == 0);
    flow_cleanup();
    puts("conntrack netlink parser integration: PASS");
    return 0;
}
