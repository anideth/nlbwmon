# Versioning and releases

> [!WARNING]
> All proposed v0.x versions are experimental. No production-readiness or reliable event-delivery guarantee is implied.

## Version identifiers

- **Source release:** SemVer prerelease tag, starting with proposed `v0.1.0-alpha.1`.
- **Development state:** the current default branch is `feature/flow-lifecycle-events`; until a tag is published, identify builds by full Git commit SHA.
- **Wire format:** `nlbwmon-flow-v1` is a separate schema identifier and does not change with every source release.
- **OpenWrt APK:** upstream package recipe version/revision and architecture are distinct from the fork's source version. Never infer fork contents from an upstream-looking APK filename.

## Release procedure

1. Complete source, documentation, and compatibility checks, and confirm the exact commit to publish.
2. Create an annotated Git tag on that commit and push it; do not move published tags.
3. Create a GitHub Release from the tag and mark every alpha/beta release as **Pre-release**.
4. Include known limitations, tested platforms, commit SHA, changes, and rollback guidance.
5. Only attach binary packages when their build provenance, OpenWrt version, target, architecture, and configuration are verified. Do not claim a universal APK.

## Planned milestones

- `v0.1.0-alpha.1`: first candidate covering experimental lifecycle export and Netlink overrun recovery; **not yet tagged or released**.
- Later alpha: Fluent Bit/OpenSearch ingestion validation and any fixes discovered there. The precise version will be chosen when scope is known.

## Safety

Events can be lost under kernel buffer overflow, network failure, queue overflow, daemon restart, or receiver failure. TCP has no built-in authentication, encryption, application-level acknowledgement, or durable sender queue. Do not use the exporter as a security audit or billing source. Back up settings and accounting data, retain the upstream package, and test rollback before deploying outside a lab.
