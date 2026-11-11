# Conntrack flow export (experimental)

This document describes the optional flow exporter added by this fork. It is **not** part of upstream nlbwmon.

## Design

nlbwmon already consumes conntrack netlink messages and periodically reads counters using `GET_CTRZERO` (read and reset). The exporter shares that path rather than independently resetting counters. `flow_export.c` tracks flow state and aggregates counter deltas from updates and destruction; `flow_json.c` formats completed records; `flow_tcp.c` implements a nonblocking, bounded TCP send queue.

The mark filter uses `(conntrack_mark & mark_mask) == (mark & mark_mask)`. The example values are `0x40000000` for both mark and mask. The mark must be set by the router's firewall/conntrack policy; this project does **not** automatically mark traffic. Flow capacity is limited by `max_flows`; exhausted capacity can lead to missing records.

## OpenWrt configuration

Use `openwrt/files/nlbwmon.config` and `openwrt/files/nlbwmon.init` together. The example section is disabled by default:

```uci
config flow_export
    option enabled '0'
    option mark '0x40000000'
    option mark_mask '0x40000000'
    option max_flows '16384'
    option tcp_host ''
    option tcp_port ''
    option tcp_queue_size '1024'
    option tcp_reconnect_interval '5'
```

Options:

| UCI option | Purpose |
| --- | --- |
| `enabled` | `0` (default) or `1`; enables flow tracking |
| `mark`, `mark_mask` | Conntrack mark match |
| `max_flows` | Maximum tracked active flows |
| `tcp_host`, `tcp_port` | Optional TCP collector; **both** required |
| `tcp_queue_size` | Maximum queued JSONL records |
| `tcp_reconnect_interval` | Reconnection interval in seconds |

The init script passes `-E`, `-M`, `-K`, `-F`, and, when both destination values are present, `-H`, `-T`, `-Q`, `-R` to the daemon. With no TCP destination, no records are sent over the network. Avoid putting secrets in UCI or committing local collector addresses to the public repository.

## JSONL format

Each completed flow is a single JSON object followed by a newline. Schema identifier: `nlbwmon-flow-v1`.

Example using **documentation-only** IP addresses:

```json
{"schema":"nlbwmon-flow-v1","id":42,"mark":1073741824,"family":2,"protocol":6,"orig_src":"192.0.2.10","orig_dst":"198.51.100.20","orig_sport":54321,"orig_dport":443,"reply_src":"198.51.100.20","reply_dst":"192.0.2.10","reply_sport":443,"reply_dport":54321,"orig_packets":8,"orig_bytes":160,"reply_packets":11,"reply_bytes":290,"started_sec":1790000000,"finished_sec":1790000060,"seen_new":true}
```

- `family`: numeric address family (`AF_INET` or `AF_INET6`).
- `protocol`: IP protocol number (e.g. 6 for TCP, 17 for UDP).
- `orig_*` and `reply_*`: conntrack original/reply tuples; may differ due to NAT.
- `orig_packets`, `orig_bytes`, `reply_packets`, `reply_bytes`: accumulated directional counters.
- `started_sec`, `finished_sec`: Unix timestamps in seconds.
- `seen_new`: whether the tracker observed a NEW event for this flow.
- `id`: conntrack identifier, **not** a globally unique persistent record ID.

The TCP collector must handle arbitrary chunking: one `recv()` can contain a partial JSON line or several lines.

## Building with OpenWrt SDK

Build with the SDK matching the **target OpenWrt version and architecture**. The SDK package Makefile can use `USE_SOURCE_DIR` to point to a local checkout. Ensure its `files/nlbwmon.init` and `files/nlbwmon.config` are copied from this fork before packaging. This is necessary because the OpenWrt package recipe installs those files from its own feed directory.

A local `USE_SOURCE_DIR` build directory may be a symlink; use `find -L` when inspecting compiled objects. Do not publish SDK binaries as portable across releases.

The APK package name/version may still reflect the upstream recipe; verify the embedded executable and configuration, and assign a distinguishable package revision before distributing releases.

## Testing

- Synthetic parser integration test (real `nfnetlink.c`, synthetic NEW/GET_CTRZERO/DESTROY messages): `sh tests/run-parser-qemu.sh`. The runner uses `OPENWRT_SDK` if set and otherwise a developer-specific default SDK path; requires `qemu-aarch64`.
- Flow accumulation, JSON formatting and TCP reconnect/reset tests live in `tests/`.
- The parser test is also exposed by CMake with `-DBUILD_FLOW_PARSER_TEST=ON`.

These tests do **not** prove correctness on a production router. Before enabling export, back up `/etc/config/nlbwmon` and the accounting database, validate package upgrade behavior, confirm only one nlbwmon process is running, and compare records against a controlled traffic sample. Do not disable an existing independent DESTROY-event collector until the replacement has been verified.

## Delivery, privacy and security

- TCP is **best effort**: no acknowledgement, retransmission after application-level failure, disk spool, or deduplication. Queue overflow and disconnections can lose records.
- TCP transport is **plaintext**. Use a trusted local segment or an independently secured tunnel. Avoid exposing the collector publicly.
- Flow records contain IP addresses, ports, connection metadata and timestamps. Treat collected records as potentially sensitive; apply retention and access controls.
- Counter-zero reads are shared state in the kernel. Running another independent zero-on-read conntrack poller can distort accounting.
- Flow coverage depends on conntrack event delivery, marks, capacity, daemon lifetime and other kernel/runtime conditions.

## Upstream contribution plan

Keep changes separated into reviewable patches: (1) parser correctness and defensive attribute handling, (2) optional flow tracking, (3) serialization and transport, (4) OpenWrt integration and tests. Upstream acceptance is not assumed.
