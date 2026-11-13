# Changelog

> [!CAUTION]
> Experimental fork. Not production-ready; flow events can be lost, duplicated, or incomplete. Do not use for billing, compliance, or security auditing.

## Unreleased — candidate for v0.1.0-alpha.1

### Added
- Optional conntrack flow lifecycle export (NEW/DESTROY) using JSONL over TCP.
- Flow correlation metadata, original/reply tuples, and directional counters.
- Bounded in-memory flow tracking and TCP output queue, with reconnect attempts.
- Optional OpenWrt configuration; exporter disabled by default.

### Fixed
- Keep the Netlink event socket registered for polling after a receive overrun, allowing subsequent event processing without daemon restart in the tested lab scenario.

### Validation
- Isolated tests included 300 sequential matched flows, 500 parallel matched flows, and a high-churn run that recorded 3,158 Netlink drops.
- After the overrun, 30 new test flows produced matching NEW/DESTROY pairs without restarting the daemon.
- These are bounded laboratory observations, **not** a guarantee of lossless delivery or production stability.

### Not yet validated
- Fluent Bit → OpenSearch ingestion, end-to-end persistence and backpressure behavior.
- Long-duration stability, comprehensive NAT/IPv6 correctness, upgrade and rollback behavior across targets.

See [lab validation](docs/lab-validation-2026-11-13.md) and [limitations](docs/limitations.md).
