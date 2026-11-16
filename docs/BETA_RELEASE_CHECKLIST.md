# OpenWrt fork deployment, resource sizing and attribution (beta preparation)

This is an **unofficial fork** of [Jo-Philipp Wich's nlbwmon](https://github.com/jow-/nlbwmon), whose source includes the ISC license in [COPYING](../COPYING). Preserve that notice in redistributions. Do not claim endorsement by upstream, OpenWrt, Prometheus, or Grafana.

## What the package does and does not install

The fork's OpenWrt APK builds a modified `nlbwmon` binary plus the package's init/config files. The source repository contains [build-nlbwmon.sh](../build-nlbwmon.sh), [deploy-nlbwmon.sh](../deploy-nlbwmon.sh), [BUILD_AND_ROLLBACK.md](BUILD_AND_ROLLBACK.md), and [FLOW_EXPORT.md](FLOW_EXPORT.md). The SDK feed's package recipe must be staged with the fork's **matching** `openwrt/files/nlbwmon.init` and `openwrt/files/nlbwmon.config`; installing only the executable will not enable the new UCI flags.

The package intentionally does **not** install third-party Lua collector code, a Grafana dashboard, a collector on Debian, OpenSearch mappings, firewall marking rules, or local site-specific addresses. Those components are independent, have different update cycles and security requirements, and should be installed/configured separately. Existing user UCI settings must not be silently overwritten on package upgrade.

## Build and target compatibility

The current scripted build targets OpenWrt **25.12.6**, using matching SDKs:
- `mediatek/filogic` / `aarch64_cortex-a53` for the tested ASUS TUF-AX4200 class;
- `x86/64` / `x86_64` for lab VMs.

The target SDK, libc/toolchain ABI, OpenWrt release and package dependencies must match the device. Do not install a Filogic APK on an arbitrary ARM router. Follow [BUILD_AND_ROLLBACK.md](BUILD_AND_ROLLBACK.md) for exact SDK layout, reproducible source commit, package identity, SHA256, lab snapshot and rollback preflight.

```sh
bash build-nlbwmon.sh --target x86-64
bash build-nlbwmon.sh --target filogic
```

For future upstream releases, use the reviewed [upstream synchronization procedure](UPSTREAM_SYNC.md); Git merge success is not a substitute for lab validation. Build scripts do not change the router. Never promise automatic rollback: the interactive deploy helper does not implement it and must be validated against a lab snapshot before general release.

## Mark/mask: where marks come from

The exporter applies `(conntrack_mark & mark_mask) == (mark & mark_mask)`. Its example values are `mark=0x40000000` and `mark_mask=0x40000000`. **The package does not set these marks.** A site-specific firewall4/nftables policy must assign a **conntrack** mark (`ct mark`, not merely packet `meta mark`) before the exporter can select marked flows. Design this policy around local firewall rules, routing and existing mark consumers; never install a universal nftables rule that may overwrite marks used by SQM, policy routing or other services.

To capture all conntrack flows without marking, set `mark_mask=0x00000000`. This intentionally increases traffic, memory pressure and OpenSearch volume; do not enable it as a silent package default. `max_flows` and `tcp_queue_size` bound in-memory structures, but overflowing either can lose records.

## Memory: 512 KiB is not 512 MiB

The fork's default `option netlink_buffer_size 524288` is **524,288 bytes = 512 KiB**, a netlink receive-buffer setting, **not** a 512 MiB RSS allocation or a minimum router RAM requirement. The procd init file passes this value to the daemon with `-b`. It is already part of the package's config template and the init script also defaults to the same value.

Total daemon RAM use also depends on the accounting database, `database_limit`, `max_flows`, TCP queue, conntrack volume, allocator overhead and system load. **No measured minimum router RAM has been established**. Do not advertise a 512 MiB minimum without benchmarks. On small devices, lower flow/table limits, watch RSS/OOM and Netlink overrun logs, and consider disabling optional export. If a particular kernel doubles/limits `SO_RCVBUF`, measure the effective buffer instead of equating the UCI number with physical process RSS.

## Independent Lua/Prometheus exporter and dashboard attribution

The optional Prometheus collector is written and maintained by **giuliomagnifico**, repository [openwrt-nlbwmon-prometheus-collector](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector). It reads the existing `nlbw -c csv` output and exposes nlbwmon metrics through OpenWrt's [prometheus-node-exporter-lua](https://github.com/openwrt/packages/tree/master/utils/prometheus-node-exporter-lua). It is **not** this fork's lifecycle TCP exporter and is not bundled in the APK.

The upstream author's [Grafana dashboard example](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector/blob/main/Grafana%20dashboard%20example.json) is a separate asset. If publishing a modified dashboard, retain attribution to the original author and a source URL; inspect the dashboard file's own license and any imported assets before redistribution. Do not copy the author's Lua code or dashboard into this repository without checking the relevant redistribution license. See [PROMETHEUS_AND_BACKUP.md](PROMETHEUS_AND_BACKUP.md) for installation, external file backup and sysupgrade survival.

Other upstream dependencies/services: [OpenWrt](https://openwrt.org/), [Linux conntrack](https://conntrack-tools.netfilter.org/), [Prometheus](https://prometheus.io/), [Grafana](https://grafana.com/), [OpenSearch](https://opensearch.org/). The independent Python collector is at [anideth/nlbwmon-aggregator](https://github.com/anideth/nlbwmon-aggregator).

## Release checklist (not yet signed off)

- [ ] Audit fork diff, preserve original ISC license, document all added-file authors and licenses.
- [ ] Verify exact build inputs and SHA256, correct init/config files in APK, package metadata and target-specific dependencies.
- [ ] Test install, restart, UCI retention and downgrade to official nlbwmon on a Proxmox lab snapshot.
- [ ] Verify mark/mask behavior with and without firewall conntrack marking and ensure no other zero-on-read conntrack consumer.
- [ ] Measure router RAM/CPU and netlink drop behavior on low-memory hardware; document tested rather than assumed minimums.
- [ ] Verify real NEW/DESTROY pairing, NAT/IPv6 behavior, overflow and reconnect loss; document best-effort limitations.
- [ ] Review privacy and security: TCP is unauthenticated plaintext; no private IPs, hostnames, keys, or site-specific firewall rules in public examples.
- [ ] Publish an immutable beta tag from the exact tested commit and a GitHub **pre-release**, with APK per target, checksum, provenance, changelog, supported firmware and rollback instructions.

Do not mark the above as complete merely because a production router has run for 12 hours.
