# Laboratory validation — dual Netlink sockets (2026-11-13)

Branch: `feature/flow-lifecycle-events`. Implementation commit: `ff11c86b2c5e3fa60604171e2c80c285ae780c07`.

## Observed PASS

- ARM64 QEMU parser integration: `conntrack netlink parser integration: PASS` (reported by operator after commit).
- x86-64 OpenWrt package built and deployed on **lab-only** router; daemon started.
- Two `NETLINK_NETFILTER` (protocol 12) sockets confirmed via `/proc/net/netlink`: event socket with groups `00000005`, dump socket with groups `00000000`; both had `Drops=0` at inspection.
- Legacy `nlbw -c csv -n` returned nonzero accounting data; no before/after numerical reconciliation was performed.
- Single 90-second iperf3 TCP test: sender ~942 Mbit/s, ~9.87 GiB; both control and data connections eventually emitted matching NEW/DESTROY IDs. Data-flow DESTROY included 10,628,130,373 orig bytes and 707,752 orig packets. Control-flow DESTROY arrived later, consistent with conntrack lifecycle.
- Sequential short-connection test: 300/300 successful TCP connects in 30.3 s. A fresh diagnostic JSONL receiver subsequently counted **NEW=300, DESTROY=300, paired=300, pending=0, orphan_destroy=0, duplicate_events=0, bad_json=0** by elapsed 210 s, remaining stable through 990 s. This is 100% pairing in this run, not a general guarantee.

## Limitations / follow-up

- These results do not establish zero loss under sustained parallel connection churn, receiver failure, or kernel Netlink receive-buffer overflow.
- Parser QEMU test does not exercise live Netlink socket handling; live tests above do.
- No sensitive network addresses, hostnames, or router identifiers are included in this report.
- Next: bounded parallel-connection churn with automatic NEW/DESTROY pairing and per-socket Netlink Drops monitoring. Separate, controlled receive-buffer pressure test only if needed; do not change production services.
