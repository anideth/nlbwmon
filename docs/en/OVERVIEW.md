# nlbwmon: project overview

**English** | [Русский](../ru/OVERVIEW.md)

An experimental OpenWrt fork of [jow-/nlbwmon](https://github.com/jow-/nlbwmon). It retains the original per-device bandwidth accounting and adds optional export of individual network connection events.

## Why this fork exists

The goal was to support two uses at once:

- **Prometheus and Grafana:** retain the existing per-device bandwidth statistics. A separate [nlbwmon Prometheus collector](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector) reads nlbwmon data and exposes metrics.
- **OpenSearch:** collect connection-level details—addresses, ports, protocol, timestamps, and traffic volumes—for search and analysis.

The initial approach used two independent conntrack collectors. This caused a conflict: reading counters in **read-and-reset** mode (`GET_CTRZERO`) resets them in the kernel. Once the first collector has read them, the second may see zeros or only a fraction of the accumulated traffic. The resulting statistics become inconsistent.

**The solution:** integrate flow export into nlbwmon's existing accounting path rather than running a second competing read-and-reset poller.

## How it works

1. nlbwmon continues ordinary per-device accounting; a third-party Prometheus collector reads those statistics for Grafana.
2. The built-in flow exporter shares nlbwmon's Linux conntrack events and counter-reading path instead of issuing competing `GET_CTRZERO` polls. A conntrack `mark/mask` filter can restrict eligible flows.
3. The exporter produces `NEW` and `DESTROY` JSONL events and sends them over TCP to a receiver.
4. The separate Python project [nlbwmon-aggregator](https://github.com/anideth/nlbwmon-aggregator) includes a lifecycle collector that receives events, persists them in SQLite, and delivers updates to OpenSearch. It also contains a separate legacy Fluent Bit aggregation path.

Flow export is disabled by default and requires no second router-side daemon. Event schema: `nlbwmon-flow-v1`.

## Limitations

- TCP delivery is plaintext and has no application-level acknowledgement. Queue overflow, disconnections, and lost kernel events may cause missing records.
- Coverage depends on conntrack, the mark/mask filter, and flow-table capacity.
- This is not a guaranteed complete record for billing, auditing, or investigations.
- Do not run a second independent `GET_CTRZERO` poller against the same counters.

## Before installing

Match the APK to the OpenWrt version and device architecture. Back up configuration and accounting data. Validate installation and rollback in a lab first.

Further reading: [export format](../FLOW_EXPORT.md), [build and rollback](../BUILD_AND_ROLLBACK.md), [limitations](../limitations.md).

> Status: experimental beta preparation; not production-ready.
