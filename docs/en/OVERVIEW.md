# nlbwmon: project overview

**English** | [Русский](../ru/OVERVIEW.md)

This is an experimental OpenWrt fork of [jow-/nlbwmon](https://github.com/jow-/nlbwmon). The original per-device bandwidth accounting remains available. The fork adds **optional** Linux conntrack flow lifecycle export.

## How it works

1. nlbwmon processes conntrack events and traffic counters through its existing Netlink path.
2. The exporter tracks connections matching the configured conntrack mark/mask filter.
3. NEW and DESTROY lifecycle events are serialized as JSONL and may be sent to a TCP receiver.

Export is disabled by default. This is neither NetFlow/IPFIX nor a separate daemon. Event schema: `nlbwmon-flow-v1`.

## Limitations

- TCP delivery is plaintext and has no application-level acknowledgement. Queue overflow, disconnections, and lost Netlink events may cause missing records.
- Coverage depends on conntrack, the mark/mask filter, and flow-table capacity.
- Do not use this as a complete or guaranteed record for billing, auditing, or investigations.
- Do not run a second independent `GET_CTRZERO` poller: it may distort counters.

## Before installing

Match the APK to the exact OpenWrt release and device architecture. Back up the configuration and accounting database. Validate installation and rollback in a lab first.

Further reading: [export format](../FLOW_EXPORT.md), [build and rollback](../BUILD_AND_ROLLBACK.md), [limitations](../limitations.md).

> Status: experimental beta preparation; not production-ready.
