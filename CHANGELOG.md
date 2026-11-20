# Changelog

> [!CAUTION]
> Experimental fork. Not production-ready; flow events can be lost, duplicated, or incomplete. Do not use for billing, compliance, or security auditing.


## v0.1.0-rc.1 — release candidate (in preparation; NOT published)

### Changed
- Export conntrack kernel timestamps with nanosecond fractions as `started_sec`/`started_nsec` and `finished_sec`/`finished_nsec`; the receiving collector can derive `date_nanos` and duration fields.
- Include `exporter_instance_id` (daemon-instance identifier) and monotonic per-process `event_sequence` for correlating and diagnosing lifecycle events. Neither guarantees delivery or detects all losses.
- Keep the wire schema identifier `nlbwmon-flow-v1` unchanged; validate consumers against the additional fields.
- Allow reproducible builds from the RC release branch; the RC branch package version is now `0.1.0_rc1-r1`; x86-64 and mediatek/filogic SDK builds were reported successful, but RC installation and rollback have not been verified.

### Observed validation and remaining gates
- An OpenWrt 25.12.6 mediatek/filogic build was installed and the standard `nlbw -c csv` accounting path remained functional. Lifecycle records with nanosecond precision reached the companion collector/OpenSearch pipeline.
- Before publishing: repeat tests and SDK builds from the final tagged commit; verify packaging/version metadata, rollback, and sustained operation. Prior TCP queue-drop logs do not establish current loss rates. Delivery is best-effort and not suitable for billing or auditing.

## v0.1.0-beta.1 — source-only beta (published 2026-11-18)

- No prebuilt APKs are provided. Build for the exact OpenWrt release, target and package architecture using the matching SDK.
- The existing experimental SDK package recipe may still use `0.1.0_alpha1-r1`; it is not a universal binary release.
- Added bilingual installation documentation, separate Lua/Prometheus/Grafana instructions, a sanitized Grafana dashboard with a tested WAN interface selector, and a collapsed dashboard screenshot in both READMEs.


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
