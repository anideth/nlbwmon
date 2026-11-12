# OpenWrt nlbwmon + Prometheus: installation, backups, upgrade recovery

This is a deployment guide for **optional, externally maintained** monitoring integration. The third-party collector is not packaged with this repository's nlbwmon APK. It reads nlbwmon's existing `nlbw -c csv` report and does not participate in the experimental conntrack NEW/DESTROY JSONL flow exporter. The external collector and Grafana dashboard are maintained by [giuliomagnifico](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector); upstream guide, screenshots, sample dashboard, and PromQL examples are in that repository.

## Install on the router

1. Install `nlbwmon`, `prometheus-node-exporter-lua`, and `uhttpd-mod-lua` (plus `uhttpd` if missing), using the packages compatible with the installed OpenWrt release. The exporter package may pull other dependencies. Use the **stock nlbwmon APK** unless intentionally testing this experimental fork.
2. Confirm `nlbw -c csv` returns data rows before diagnosing Prometheus.
3. Install the author's collector (run on OpenWrt):

```sh
mkdir -p /usr/lib/lua/prometheus-collectors && wget -O /usr/lib/lua/prometheus-collectors/nlbwmon.lua https://raw.githubusercontent.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector/main/nlbwmon.lua && /etc/init.d/prometheus-node-exporter-lua restart
```

4. Verify the endpoint on whichever address/port the exporter actually listens on. The author's example is `http://127.0.0.1:9100/metrics?collect[]=nlbwmon`; on some OpenWrt configurations the exporter listens on a specific interface rather than loopback, so replace the address with the configured reachable one:

```sh
wget -qO- 'http://ROUTER_ADDRESS:9100/metrics?collect[]=nlbwmon' | grep -E 'nlbwmon_|node_scrape_collector_success' | head
```

Expect `nlbwmon_rx_bytes`, `nlbwmon_tx_bytes`, `nlbwmon_connections` with `family,proto,port,mac,ip,hostname,layer7` labels and a successful collector scrape. The stats are cumulative for the current nlbwmon accounting period and **reset** at period rollover, daemon/database resets, or restore. Do not interpret short-interval Grafana `rate()` from an empty/new database as an accounting failure. The author's sample Grafana dashboard is in [Grafana dashboard example.json](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector/blob/main/Grafana%20dashboard%20example.json).

If the exporter's HTTP endpoint is working but nlbwmon metrics are absent, check the manually installed Lua file and scrape status **before** touching conntrack, netlink, sysctls, nftables, or firmware. Do not replace the author-maintained collector with an untested script. If exporter refuses to start after an upgrade, inspect `logread`, installed `uhttpd` and `uhttpd-mod-lua` versions and the corresponding Lua module, rather than blindly reinstalling networking packages.

## Preferred firmware update workflow (OpenWrt 25.12.x / APK / owut)

On the tested OpenWrt 25.12.6 router, use `owut` to request a firmware image with a compatible package set, rather than flashing a generic image and then manually attempting to reconstruct packages. **This is still not a full backup**, and packages outside the image builder's known repositories, plus manually copied files, are not guaranteed to be included.

Run on the router, one step at a time:

```sh
owut check
```

Review the proposed OpenWrt version, kernel, package changes and any missing or incompatible packages. Do not proceed until errors and unexpected removals are understood.

```sh
owut download
```

Build/download the candidate image. Inspect the result and ensure the intended image is available before upgrading.

```sh
sysupgrade -b /tmp/openwrt-backup.tgz
```

**Copy the backup archive off the router** and verify its contents before rebooting. Save the installed package inventory and any custom scripts, APKs, firmware and accounting databases separately. Check `sysupgrade -l` for the external Prometheus collector and other manually installed files. `/tmp` is ephemeral.

```sh
owut upgrade
```

Only after confirming the image, backup and package plan. Confirm which image `owut upgrade` will flash; do not assume it necessarily uses the previously downloaded file. Keep the backup and recovery path available. After boot, follow the verification checklist below. **Do not use `sysupgrade -n`** when retaining configuration is required. Test any restore on the matching firmware release and do not blindly restore incompatible system libraries or old package binaries.

## Backup before sysupgrade

OpenWrt `sysupgrade` with configuration retention **is not a full filesystem image**. It can preserve configuration files but drops manually installed application code in `/usr/lib` and installed packages may need reinstalling. Always do a preflight and store the archives **outside the router**, ideally in versioned, access-controlled storage. Take care: UCI configs, hostnames, IPs, DHCP leases, passwords, private keys and network topology may be sensitive; do not commit unredacted backups to a public repository.

On OpenWrt, preserve the external collector in retained configuration **before** the next upgrade:

```sh
grep -qxF '/usr/lib/lua/prometheus-collectors/nlbwmon.lua' /etc/sysupgrade.conf 2>/dev/null || echo '/usr/lib/lua/prometheus-collectors/nlbwmon.lua' >> /etc/sysupgrade.conf
```

Capture an inventory and verify the backup archive contains all required files:

```sh
apk info -vv > /tmp/openwrt-packages-before-upgrade.txt
sysupgrade -l | grep -E 'nlbwmon.lua|sysupgrade.conf|/etc/config/(nlbwmon|prometheus-node-exporter-lua)'
sysupgrade -b /tmp/openwrt-config-backup.tar.gz
tar -tzf /tmp/openwrt-config-backup.tar.gz | grep -E 'nlbwmon.lua|sysupgrade.conf|etc/config/(nlbwmon|prometheus-node-exporter-lua)'
```

These examples use OpenWrt with APK package management; on older OpenWrt releases use `opkg list-installed` instead. The package inventory in `/tmp` is **not included in sysupgrade backup by default**: transfer it separately, or explicitly add a persistent inventory file to the backup set. Likewise copy `/tmp/openwrt-config-backup.tar.gz` off-device before flashing; `/tmp` is temporary. Run `sysupgrade -l` and inspect `tar` output before proceeding. If nlbwmon's accounting database matters, separately back up its actual configured `database_directory` and verify its archive; do not assume that it is under `/etc` or retained. A consistent snapshot can be made after an `nlbw -c commit`, when supported, but avoid simultaneous writers and check the result. Preserve any customized package feed, APK files/versions, and rollback images outside the router.

Make a reviewed inventory of **all manually installed non-package files**, not merely the collector; add only the files that actually need restoring to `/etc/sysupgrade.conf`. Do not blindly back up or restore the whole `/usr`, kernel modules, libraries or firmware files across OS upgrades. Ensure custom paths and their dependencies are compatible with the destination version. Keep critical credentials in a protected offline backup rather than public Git.

## After sysupgrade: read-only verification first

1. Confirm exact OpenWrt release, installed packages and services before reinstalling anything: `cat /etc/openwrt_release`, `apk info -vv`, `/etc/init.d/nlbwmon status`, `/etc/init.d/prometheus-node-exporter-lua status`.
2. Check the collector exists: `ls -l /usr/lib/lua/prometheus-collectors/nlbwmon.lua`.
3. Check nlbwmon's accounting directly: `nlbw -c csv | head` and compare its counters after a controlled download; do not rely only on a Grafana panel.
4. Check kernel state non-destructively: `sysctl net.netfilter.nf_conntrack_acct net.netfilter.nf_conntrack_events net.netfilter.nf_conntrack_count net.netfilter.nf_conntrack_max net.core.rmem_max`. Event mode `2` is automatic and is not itself proof of lost events. Check any flow-offload settings before attributing loss to them.
5. Inspect exporter endpoint and collector success, then the configured Prometheus scrape target and Grafana queries. Distinguish target-down, missing collector, empty `nlbw`, and missing Grafana series. Verify metadata and units; `rate()` requires at least two datapoints.
6. If components or packages are missing, restore **only those confirmed missing or incompatible**, checking package versions and the saved inventory first.
7. Re-check saved UCI configuration, DHCP/hostnames, service autostart, monitoring and router reachability. Avoid replacing working configs during diagnosis.

## Lessons learned

- The stock nlbwmon CSV-based Prometheus collector is a separate deployment artifact, not part of our experimental APK. Losing it does not mean conntrack or nlbwmon itself stopped counting.
- A sysupgrade with `-c`/retained settings is not a complete backup. Verify **contents**, package inventory, custom files, and restore path; do not use `sysupgrade -n` when the requirement is to keep configuration.
- Do not confuse an experimental lifecycle-flow modification with Prometheus exporter behavior. Avoid changing kernel sysctls, nftables rules or reinstalling networking packages until the data path and failing stage are demonstrated.
- Keep documented rollback instructions, the original stock package and a copy of external dependencies. Preserve the author's upstream attribution and URL; review changes before updating remotely fetched code.
