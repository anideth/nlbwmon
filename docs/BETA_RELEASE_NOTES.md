# Draft beta release notes — nlbwmon lifecycle fork

**Status: DRAFT / NOT PUBLISHED.** Version and immutable tag must be chosen only after testing the exact release commit and building both target APKs. Do not equate the earlier `0.1.0_alpha1-r1` package with a beta build.

## What changed from upstream
- Experimental conntrack lifecycle `nlbwmon-flow-v1` JSONL export: NEW/DESTROY events, per-flow connection identity, counters and router identity.
- Best-effort TCP sender, bounded queue and configurable conntrack mark/mask filter.
- Netlink event socket recovery after overrun; events already lost during an overrun cannot be reconstructed.
- OpenWrt UCI/procd integration and SDK build helper for x86/64 and mediatek/filogic.

## What remains upstream / external
- Original [nlbwmon](https://github.com/jow-/nlbwmon), copyright Jo-Philipp Wich, ISC license in [COPYING](../COPYING).
- Independent [Python collector](https://github.com/anideth/nlbwmon-aggregator) for SQLite/OpenSearch lifecycle indexing.
- Independent [Lua Prometheus collector](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector) and author's Grafana example. Neither is bundled in this APK.

## Supported scope
OpenWrt 25.12.6 SDK targets `x86/64` and `mediatek/filogic` only, subject to verification of actual APKs. Other targets and firmware versions are not claimed compatible. No lossless delivery, minimum-RAM guarantee or universal NAT attribution claim.

## Installation and rollback
Read [BUILD_AND_ROLLBACK.md](BUILD_AND_ROLLBACK.md), [FLOW_EXPORT.md](FLOW_EXPORT.md) and [BETA_RELEASE_CHECKLIST.md](BETA_RELEASE_CHECKLIST.md). The lifecycle collector's [end-to-end guide](https://github.com/anideth/nlbwmon-aggregator/blob/release/beta-preparation/docs/INSTALLATION_END_TO_END.md) covers OpenSearch and systemd. Preserve existing accounting data and UCI settings. The deploy helper does not provide automatic rollback.

## Publication gate
- [ ] Verify exact source commit, toolchain/feeds and source provenance.
- [ ] Build both APKs; record actual filenames, APK metadata, SHA256 and buildinfo.
- [ ] Validate installation, rollback and config retention on a lab snapshot; user reports earlier tests, but logs/commands must be attached to release evidence.
- [ ] Check security/privacy of **all branches and relevant Git history** before making any repository public.
- [ ] Verify end-to-end NEW->DESTROY, queue and PTR behavior.
- [ ] Create immutable beta tag, attach verified binaries, checksums and changelog; mark as GitHub pre-release.

**Next release:** optional configurable CIDR exclusions belong to the server-side collector and are deliberately outside this beta.
