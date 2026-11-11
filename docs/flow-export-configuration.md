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

## Planned UCI interface (not yet wired into init)

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

Only the first four settings are implemented in the daemon. The remaining
settings are reserved for subsequent stages and **must not** be considered
functional yet. The OpenWrt package init script currently does not pass the
new flags; simply adding the UCI section does not enable tracking.

## Operational caveats

* There is no network delivery yet; the completed-flow callback is currently
  used only by the unit tests.
* The active-flow cap rejects new entries with `ENOSPC`; it is not a durable
  queue or a guarantee against data loss.
* The module does not yet persist state across daemon restarts.
* Duplicate DESTROY events and aging of orphaned flows remain to be addressed.
* Do not replace the existing conntrack collector until end-to-end delivery
  and reconciliation have been verified.
