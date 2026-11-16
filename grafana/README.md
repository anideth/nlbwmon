# OpenWrt Grafana dashboard (beta companion)

This optional dashboard is **not part of the nlbwmon APK**. It visualizes OpenWrt node-exporter Lua metrics and, in three panels, traffic statistics from the **external** nlbwmon Prometheus collector. It does not visualize the new conntrack flow JSON events directly.

## Traffic dashboard preview

The locally prepared screenshot covers four customized panels: **WAN Traffic (RX / TX)**, **Network Traffic by Device (RX / TX)**, **Top Download by Device**, and **Top Upload by Device**. It was captured in Grafana with device names blurred. The PNG is awaiting upload to `grafana/screenshots/traffic-overview.png`; do not treat the placeholder path as a published asset yet.

## Attribution and license

Based on [OpenWRT dashboard #11147](https://grafana.com/grafana/dashboards/11147/) from [try2codesecure/grafana_dashboards](https://github.com/try2codesecure/grafana_dashboards/tree/master/OpenWRT), originally distributed under the MIT license. Original license and copyright notice are preserved in [LICENSE-MIT](LICENSE-MIT). This is an independently modified dashboard, not an official upstream release or endorsement. Changes were made in Grafana's web editor over time and exported for beta distribution.

The Prometheus Lua collector is **separate third-party software**: [giuliomagnifico/openwrt-nlbwmon-prometheus-collector](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector). It is not bundled or maintained by this fork. Check that project's license separately before redistributing its code.

## Dashboard JSON and import

The cleaned Grafana Dashboard v2 JSON is being prepared as `openwrt-nlbwmon.json`. **It has not been committed yet**: the exported file needs an actual import test before publication. The working export uses `apiVersion: dashboard.grafana.app/v2` and is not interchangeable with every classic Grafana JSON import route.

The reviewed local distribution copy replaces 125 occurrences of a site-specific Prometheus datasource UID with `REPLACE_WITH_PROMETHEUS_DATASOURCE_UID`, clears saved `job`, `name`, and `node` selections, and keeps all 69 panels and their PromQL expressions. Before importing, replace the placeholder with your target Prometheus datasource UID and choose the desired job/host in Grafana. No production Grafana or router configuration is changed by these preparations.

The reviewed local JSON adds a `$wan_device` query variable based on `node_network_receive_bytes_total`, and both WAN Traffic queries use `device=~"$wan_device"` instead of the fixed `device="pppoe-wan"`. Select the actual WAN interface in Grafana after importing; the query relies on the selected node exporting `node_network_receive_bytes_total` with a `device` label. The device traffic panels remain unchanged. Note that a physical WAN interface and PPPoE logical interface may report different byte counts.

The dashboard uses OpenWrt node-exporter Lua metrics and optional nlbwmon metrics; missing collectors will yield empty panels. It does not require or configure the Python flow aggregator.

## Release checklist

- [x] Establish original source and MIT attribution.
- [x] Scan exported JSON for literal IPv4 addresses, URLs, common hostnames, and credential markers.
- [x] Remove saved site-specific variable values and datasource UID in the local distribution copy.
- [x] Review the four WAN/device traffic panels for site-specific host and interface filters.
- [ ] Test import into a separate Grafana instance, including datasource mapping and variable refresh.
- [ ] Commit sanitized dashboard JSON only after the import test.
- [ ] Confirm license attribution in any public release notes.

Do not publish the original unredacted export. Always inspect subsequent exports before committing them.
