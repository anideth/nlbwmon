# nlbwmon v0.1.0-beta.1 — source-only beta

> **Release candidate notes.** The annotated Git tag and GitHub Release must be created after final verification. This document does not itself publish a release.

This is an **unofficial experimental fork** of nlbwmon, adding optional conntrack lifecycle (NEW/DESTROY) JSONL-over-TCP export while retaining standard per-device accounting. Flow events are received and forwarded to OpenSearch by the separate [nlbwmon-aggregator](https://github.com/anideth/nlbwmon-aggregator) project. A third-party Lua collector by [giuliomagnifico](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector) independently exposes nlbwmon accounting to Prometheus/VictoriaMetrics/Grafana.

## Distribution and installation

**Source code only; no APK binaries attached.** Build the package for your exact OpenWrt version, target and architecture using a matching SDK and the documented recipe. Lab testing covered OpenWrt 25.12.6 x86/64. A MediaTek Filogic build was produced but has **not** been deployed to the production router. Do not treat these two targets as a universal compatibility list.

See [INSTALL.md](INSTALL.md), [INSTALL.ru.md](INSTALL.ru.md), [BUILD_AND_ROLLBACK.md](docs/BUILD_AND_ROLLBACK.md), and [FLOW_EXPORT.md](docs/FLOW_EXPORT.md). The experimental SDK package version `0.1.0_alpha1-r1` is not the source Git release version `v0.1.0-beta.1`.

## Highlights

- Optional `nlbwmon-flow-v1` lifecycle export, bounded queue and recovery after observed Netlink overrun in lab.
- Independent per-device Prometheus metrics remain available through a third-party Lua collector, not bundled with this fork.
- [Adapted Grafana dashboard](grafana/openwrt-nlbwmon-dashboard.json) with user-tested `WAN interface` selector, plus [collapsed screenshot](README.md#prometheus-per-device-traffic-monitoring-optional). Replace `PROMETHEUS_DATASOURCE_UID` with your own datasource UID before importing.
- English/Russian documentation, reproducible SDK build guidance and manual rollback precautions.

## Important limitations

Experimental, **not production-ready**. NEW/DESTROY events may be lost or duplicated; there is no delivery acknowledgement or guarantee of complete flow accounting. Not suitable as the sole source for billing, investigations, compliance or security auditing. TCP transport to the aggregator has no built-in encryption or authentication. Preserve backups and test rollback in a lab. No independent security audit or broad compatibility testing has been performed.

## Authorship and license

Fork based on [Jo-Philipp Wich's nlbwmon](https://github.com/jow-/nlbwmon), preserving upstream license and credits. The Lua collector and its original dashboard example are third-party work by giuliomagnifico; they are **not bundled**. Changes to this fork were developed with substantial AI assistance and reviewed/tested in a personal lab. The adapted Grafana dashboard comes from a user-supplied export; its full upstream panel attribution has not been independently audited.
