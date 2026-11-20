# Installation — experimental nlbwmon fork

**English** | [Русский](INSTALL.ru.md)

> [!CAUTION]
> **Experimental prerelease, not production-ready.** Flow records may be lost or incorrect. Do not use them for billing, compliance or security auditing. Back up the router configuration and accounting database, and test rollback on a lab device before installing on a working router.

This fork replaces the **official nlbwmon package** with an experimental build. It retains standard per-device accounting and adds optional JSONL NEW/DESTROY flow export. The Prometheus collector is separate; the flow receiver is the Python [nlbwmon-aggregator](https://github.com/anideth/nlbwmon-aggregator) project.

## 1. Check compatibility and prepare

The current build workflow targets **OpenWrt 25.12.6** on **x86/64** (lab) and **mediatek/filogic** (ARM64). Do not install the same APK on another target or release. This beta is distributed as source only; no prebuilt APKs are provided for any router target.

On the router, inspect without changing anything:

```sh
cat /etc/openwrt_release; apk list --installed nlbwmon; nlbw -c csv | head
```

Back up `/etc/config/nlbwmon`, the accounting database, and package information. Prefer a VM snapshot for the lab and a tested recovery method for hardware. Confirm that the original package can be obtained from your OpenWrt repositories **before** replacing it.

## 2. Build the APK

On a Linux x86-64 build host, install the **matching OpenWrt 25.12.6 SDK** (not ImageBuilder) for the target, with the `packages` feed and `CONFIG_PACKAGE_nlbwmon=m`. The supplied builder expects the extracted SDK under `~/projects/openwrt-sdk/`, and a clean committed checkout on `feature/flow-lifecycle-events`. Review the script before running it.

```sh
bash build-nlbwmon.sh --target x86-64
bash build-nlbwmon.sh --target filogic
```

Run only the command for the desired target. The script stages the source, adjusts the SDK recipe, includes the fork's **matching UCI config and init script**, and writes the APK, SHA256 and build provenance under `~/projects/openwrt-packages/25.12.6/<target>/`. Building does **not** install anything on the router.

The fork package version is `0.1.0_alpha1-r1`, distinct from the upstream package version. Check the resulting APK's **architecture, package metadata and SHA256**, not only its filename. Full prerequisites and caveats: [build and rollback](docs/BUILD_AND_ROLLBACK.md).

## 3. Install on the lab router first

The interactive deployment script is provided for evaluation; **installation and rollback have not yet been validated end-to-end**. Copy the placeholder configuration, edit it locally, and keep it out of Git:

```sh
cp deploy-nlbwmon.conf.example deploy-nlbwmon.local.conf && vim deploy-nlbwmon.local.conf
bash deploy-nlbwmon.sh
```

Choose the correct platform and **Check only** first. The script also offers **Deploy alpha** and **Roll back to official repository package**. It checks release/architecture, creates a local backup, and verifies the transferred APK checksum. It **does not automatically roll back** after failure. Do not assume it can restore a consistent live database.

After installation, verify the package, the running service, and unchanged accounting:

```sh
apk list --installed nlbwmon; /etc/init.d/nlbwmon status; nlbw -c csv | head
```

Do not overwrite a working `/etc/config/nlbwmon` merely to add the export section. Check that the installed init script and configuration match the fork.

## 4. Configure optional flow export

The reference example lives in [openwrt/files/nlbwmon.config](openwrt/files/nlbwmon.config); edit the **existing** router configuration using `vim /etc/config/nlbwmon`. Do not duplicate an existing `flow_export` section.

```uci
config flow_export
    option enabled '0'
    option router_name 'lab-router'
    option mark '0x40000000'
    option mark_mask '0x40000000'
    option max_flows '16384'
    option tcp_host ''
    option tcp_port ''
    option tcp_queue_size '1024'
    option tcp_reconnect_interval '5'
```

This is an **illustrative disabled configuration**, not a ready-to-enable policy. To export, configure the correct collector IP/port and enable it **only after the receiver is ready**. A nonzero `mark_mask` requires an appropriate conntrack marking policy; the fork does not create one. A zero mask matches all marks and may greatly increase event volume. Read [flow export configuration](docs/FLOW_EXPORT.md) before changing filters. Restart only after checking the live configuration.

## 5. Install the receiving collector

For individual NEW/DESTROY records in OpenSearch, follow the companion project's **[lifecycle collector INSTALL](https://github.com/anideth/nlbwmon-aggregator/blob/main/INSTALL.md)** ([Русский](https://github.com/anideth/nlbwmon-aggregator/blob/main/INSTALL.ru.md)). That repository is **not yet public**; these links will become accessible to others after its release. The lifecycle collector normally listens on **TCP 5517**; do not confuse it with the legacy Fluent Bit aggregator on **5515**. Confirm the actual configured listener before setting `tcp_port`.

Router-to-collector TCP is **plaintext, unauthenticated and best-effort**. Use a trusted network with firewall restrictions or a secure tunnel; never expose it openly to the Internet. Check controlled NEW/DESTROY events in OpenSearch before relying on the pipeline.

## 6. Rollback and limitations

The interactive script can attempt an explicit reinstall of the **official** nlbwmon package, but its downgrade procedure remains unverified. Keep the original package and backups available. Test rollback on the lab VM first; do not blindly replace a live database with an old snapshot.

The Prometheus Lua collector is **not included** in this APK. For its separate installation and firmware-upgrade precautions, see [Prometheus and backup](docs/PROMETHEUS_AND_BACKUP.md). For exporter delivery and privacy limitations, see [limitations](docs/limitations.md).

**Release gate:** do not publish an installable beta until clean build, lab installation, end-to-end NEW/DESTROY verification, rollback, and public-data audit are complete.

## 7. Concurrent Prometheus metrics and Grafana dashboard

OpenSearch connection export **does not disable** normal nlbwmon accounting. The independent Lua collector by giuliomagnifico reads `nlbw -c csv` and exposes `nlbwmon_rx_bytes`, `nlbwmon_tx_bytes`, and `nlbwmon_connections` through `prometheus-node-exporter-lua`. It is **third-party software**, not bundled in this APK.

See [collector installation, backup and Grafana import guide](docs/PROMETHEUS_AND_BACKUP.md). The [adapted Grafana dashboard JSON](grafana/openwrt-nlbwmon-dashboard.json) supports a `WAN interface` selector (default `pppoe-wan`). Replace `PROMETHEUS_DATASOURCE_UID` with your own Prometheus/VictoriaMetrics datasource UID before import.
