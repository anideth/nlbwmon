# Versioning and releases

> [!WARNING]
> All proposed v0.x versions are experimental. No production-readiness or reliable event-delivery guarantee is implied.


## RC preparation (not yet published)

- Target source tag: `v0.1.0-rc.1`; release branch: `release/v0.1.0-rc.1`. No tag or GitHub Release exists as part of this preparation step.
- Source tag, wire schema (`nlbwmon-flow-v1`) and OpenWrt APK package version are **different identifiers**.
- The release branch recipe now specifies `0.1.0_rc1-r1`; both x86-64 and mediatek/filogic SDK builds were reported successful. Verify APK metadata, provenance and rollback before publication. Do not label an old APK as RC.
- The build script accepts the beta2 development branch or the RC release branch; use the final Git commit SHA in build provenance.

## Version identifiers

- **Source release:** SemVer prerelease tag, starting with published source-only beta [`v0.1.0-beta.1`](https://github.com/anideth/nlbwmon/releases/tag/v0.1.0-beta.1).
- **Development state:** the current default branch is `feature/flow-lifecycle-events`; until a tag is published, identify builds by full Git commit SHA.
- **Wire format:** `nlbwmon-flow-v1` is a separate schema identifier and does not change with every source release.
- **OpenWrt APK:** the RC branch SDK recipe identifies the experimental APK as `0.1.0_rc1-r1` (candidate); this does **not** change automatically when the source-only beta version is assigned, declared in `openwrt/fork-version.mk`; the official OpenWrt feed currently uses `2025.06.02~29236be6-r1`. Both packages retain the name `nlbwmon`. The fork's version must be applied to the SDK package recipe **before** building; merely adding this file does not change SDK outputs. Verify the built APK metadata and filename for every architecture.

## Release procedure

1. Complete source, documentation, and compatibility checks, and confirm the exact commit to publish.
2. Create an annotated Git tag on that commit and push it; do not move published tags.
3. Create a GitHub Release from the tag and mark every alpha/beta release as **Pre-release**.
4. Include known limitations, tested platforms, commit SHA, changes, and rollback guidance.
5. Only attach binary packages when their build provenance, OpenWrt version, target, architecture, and configuration are verified. Do not claim a universal APK.
6. Before installing, save the active `/etc/config/nlbwmon`, accounting database and installed package identity. Validate official repository availability and a tested explicit-version rollback on the same OpenWrt target. The official init script ignores the fork-only `flow_export` section; preserve it across rollback, and do not overwrite current accounting data with an old backup during a normal rollback.
7. Never publish a GitHub Release or APK until actual build, install, and rollback tests have passed.

## Planned milestones

- `v0.1.0-beta.1`: first published source-only beta covering experimental lifecycle export, Netlink overrun recovery and bilingual installation documentation. No APK assets.
- Later alpha: Fluent Bit/OpenSearch ingestion validation and any fixes discovered there. The precise version will be chosen when scope is known.

## Safety

Events can be lost under kernel buffer overflow, network failure, queue overflow, daemon restart, or receiver failure. TCP has no built-in authentication, encryption, application-level acknowledgement, or durable sender queue. Do not use the exporter as a security audit or billing source. Back up settings and accounting data, retain the upstream package, and test rollback before deploying outside a lab.
