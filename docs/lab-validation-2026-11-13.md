# Laboratory validation — dual Netlink sockets (2026-11-13)

Initial implementation: `ff11c86b2c5e3fa60604171e2c80c285ae780c07`. Recovery fix: `49adfe064074cb6c9681a2e0e218ed99f3cbe8e6`.

## Observed PASS

- ARM64 QEMU parser integration: `conntrack netlink parser integration: PASS` (reported by operator after commit).
- x86-64 OpenWrt package built and deployed on **lab-only** router; daemon started.
- Two `NETLINK_NETFILTER` (protocol 12) sockets confirmed via `/proc/net/netlink`: event socket with groups `00000005`, dump socket with groups `00000000`; both had `Drops=0` at inspection.
- Legacy `nlbw -c csv -n` returned nonzero accounting data; no before/after numerical reconciliation was performed.
- Single 90-second iperf3 TCP test: sender ~942 Mbit/s, ~9.87 GiB; both control and data connections eventually emitted matching NEW/DESTROY IDs. Data-flow DESTROY included 10,628,130,373 orig bytes and 707,752 orig packets. Control-flow DESTROY arrived later, consistent with conntrack lifecycle.
- Sequential short-connection test: 300/300 successful TCP connects in 30.3 s. A fresh diagnostic JSONL receiver subsequently counted **NEW=300, DESTROY=300, paired=300, pending=0, orphan_destroy=0, duplicate_events=0, bad_json=0** by elapsed 210 s, remaining stable through 990 s. This is 100% pairing in this run, not a general guarantee.

- Parallel TCP connection test (2026-11-13): dedicated lab TCP listener, 500/500 successful connects with 20 worker threads in 0.04 s (client-reported elapsed). Filtered the saved JSONL by test source, destination, and destination port, then grouped by `connection_id`: **NEW=500, DESTROY=500, paired=500, pending=0, orphan=0, unique=500**. Receiver-wide counts included unrelated events from other flows; the filtered counts establish complete pairing for this test. This is a single bounded burst, not a sustained load test.

## Limitations / follow-up

- These results do not establish zero loss under sustained parallel connection churn, receiver failure, or kernel Netlink receive-buffer overflow.
- Parser QEMU test does not exercise live Netlink socket handling; live tests above do.
- No sensitive network addresses, hostnames, or router identifiers are included in this report.
- Next: sustained or repeated parallel connection churn, with per-socket Netlink Drops monitoring and exporter queue/receiver failure behavior. Separate, controlled receive-buffer pressure test only if needed; do not change production services.

## Netlink overrun recovery validation (2026-11-13)

- Before the recovery fix, a high-churn test caused Netlink receive drops and stopped subsequent event consumption while the daemon remained running. The event descriptor was no longer registered for polling.
- After the fix, a 500-connection test completed with all 500 test connections successful. Receiver-wide counters reached NEW=504, DESTROY=511, paired=504, pending=0, orphan_destroy=7, duplicate_events=0, bad_json=0; unrelated traffic was not filtered in this run.
- A subsequent 10,000-attempt stress test (20 workers, target 200 attempts/s) completed in 125.14 s with 9,683 successful connections and 317 client timeouts. The Netlink receive-drop counter reached 3,158, but the daemon retained its event polling registration, the receive queue drained to zero, and no daemon restart was required.
- A fresh receiver was started without restarting the daemon. Thirty further short TCP connections produced 30 additional NEW events and later 30 matched NEW/DESTROY pairs. This confirms that event delivery resumed after the overrun in this lab run.
- The recovery fix prevents the observed permanent loss of event processing; it **does not guarantee lossless event delivery** during Netlink overflow. Client timeouts may reflect limitations of the test listener and are not evidence of exporter performance improvement or regression.
- Next integration phase: transport exported events through Fluent Bit into OpenSearch, validating JSON parsing, field mapping, reconnect behavior, duplicates, and ingestion under backpressure in a separate lab test.
