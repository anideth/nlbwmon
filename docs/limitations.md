# Flow exporter: guarantees and limitations

Status: experimental feature branch, not a production-ready audit or billing system. These statements describe the current implementation, not a future roadmap.

## Delivery semantics (current)

- Transport: newline-delimited JSON over unencrypted, unauthenticated TCP.
- **Best-effort only.** There is no application-level acknowledgement (ACK), disk-backed queue, or end-to-end deduplication.
- The sender removes a record from its in-memory queue after the complete JSONL line has been accepted by the local TCP socket via `send()`. This does **not** prove that the receiver processed or persisted it.
- A partially sent head record is retried from byte zero after reconnection. Receivers must discard incomplete lines when a connection closes; replay and boundary failures can produce duplicates or missing data.
- Reconnection is automatic, subject to the configured retry interval. TCP availability does not guarantee collector health.
- The queue is bounded by `tcp_queue_size`; when full, new records are dropped and a `Flow TCP queue: dropped=..., queued=...` message can be logged. Queue contents are lost when the daemon exits or the router reboots.
- The active flow table is separately bounded by `max_flows`. Overflow, kernel conntrack event loss, restarts, mark filtering, and counter sampling can affect coverage and accuracy.

## Event and accounting semantics

- The `nlbwmon-flow-v1` stream includes lifecycle information such as `event_type`, `connection_id`, `seen_new`, and original/reply tuples. A `NEW` event can legitimately contain zero counters.
- Conntrack counters and NAT tuples are not equivalent to an application request log; do not assume every connection has a complete NEW/DESTROY pair.
- Do not run independent `GET_CTRZERO` readers against the same conntrack table: zero-on-read is shared kernel state.
- Exporter records and the original nlbwmon accounting database serve different purposes; enabling the optional exporter must not be treated as a substitute for the built-in accounting database.
- The receiver decides which traffic is in scope. A record received over TCP may be intentionally rejected by the receiver's configurable address, mark, protocol, or direction filters. Rejected is not synonymous with transport loss.
- Avoid simultaneously ingesting legacy conntrack DESTROY text and the new JSONL representation of the same flow into a single accounting pipeline: no exactly-once reconciliation exists.

## Security and privacy

- TCP is plaintext and has no built-in peer authentication. Restrict listeners to trusted interfaces and firewall rules or provide a separately authenticated, encrypted tunnel.
- Records can reveal private/public IP addresses, ports, host/router names, timestamps and communication patterns. Protect logs and sample configurations, control access and retention, and sanitize bug reports.
- Do not publish production addresses, credentials, collector endpoints, or identifying hostnames in examples.

## Tested vs unverified

Observed in an isolated OpenWrt lab: JSONL NEW events, idle TCP reconnect after receiver shutdown, bounded queue with `-Q 8` (8 queued / 22 dropped in one test), and delivery of 8 queued lines after the receiver restarted. All 8 lines were rejected by the shadow classifier because that particular test used traffic between configured local networks; this is **not** evidence of 8 accepted/accounted flows.

Not yet established: loss-free operation under arbitrary socket failures, end-to-end delivery guarantees, absence of duplicates, long-duration accounting correctness, all IPv6/NAT topologies, package upgrade/rollback across targets, and production safety.

## Future design (not implemented)

An optional reliable mode may add receiver acknowledgements, bounded retransmission, unique event IDs and receiver-side idempotency. ACK alone cannot guarantee survival across router reboot or collector storage failure. Any claim of durable delivery would additionally require durable state and tested recovery semantics. Lightweight best-effort export should remain available.

See [FLOW_EXPORT.md](FLOW_EXPORT.md) for configuration and schema, and the companion aggregator repository for receiver-specific semantics.
