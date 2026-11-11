# Optional conntrack flow accounting (development)

The flow-accounting extension is disabled by default. It does not change the
existing nlbwmon database or its Prometheus exporter.

## Daemon options implemented

| Flag | Meaning | Default |
| --- | --- | --- |
| `-E 0|1` | Enable marked-flow tracking | `0` |
| `-M NUMBER` | Conntrack mark value, decimal or hex | `0x40000000` |
| `-K NUMBER` | Conntrack mark mask, decimal or hex | `0x40000000` |
| `-F NUMBER` | Maximum simultaneous tracked flows (1–1048576) | `16384` |

Matching uses `(conntrack_mark & mark_mask) == mark`.
A mark containing bits outside the mask is rejected.

Example daemon flags (not yet integrated with OpenWrt init):

```sh
-E 1 -M 0x40000000 -K 0x40000000 -F 16384
```

## OpenWrt UCI interface (procd overlay available)

```text
config flow_export
    option enabled '0'
    option mark '0x40000000'
    option mark_mask '0x40000000'
    option max_flows '16384'
    option stale_timeout '3600'
    option format 'json'
    option transport 'tcp'
    option host '127.0.0.1'
    option port '5515'
    option queue_size '4096'
    option reconnect_interval '5'
```

Only the first four settings are implemented. The remaining settings are
reserved for subsequent stages and **must not** be considered functional yet.

The fork contains `openwrt/files/nlbwmon.init` and
`openwrt/files/nlbwmon.config`. These are **packaging overlays**, not files
automatically installed from the CMake project. To build a package with the
UCI integration, copy the init overlay into the SDK package recipe's
`feeds/packages/net/nlbwmon/files/nlbwmon.init` before building. Do not
overwrite an existing router `/etc/config/nlbwmon` during testing; add the
`flow_export` section manually if missing. The default section in the
overlay config is disabled.

Example (run from the fork root, after locating your SDK):

```sh
cp openwrt/files/nlbwmon.init /path/to/sdk/feeds/packages/net/nlbwmon/files/nlbwmon.init
```

The existing UCI config remains authoritative on upgrades. A procd restart
is needed after changing the flow_export section.

## Operational caveats

* There is no network delivery yet; the completed-flow callback is currently
  used only by the unit tests.
* The active-flow cap rejects new entries with `ENOSPC`; it is not a durable
  queue or a guarantee against data loss.
* The module does not yet persist state across daemon restarts.
* Duplicate DESTROY events and aging of orphaned flows remain to be addressed.
* Do not replace the existing conntrack collector until end-to-end delivery
  and reconciliation have been verified.
