# Building the experimental OpenWrt APK

This is an **experimental** fork. It is not a production-ready or lossless flow-audit solution.

## Package identity

- Git release candidate: `v0.1.0-alpha.1` (not published).
- APK version: `0.1.0_alpha1-r1`, configured in `openwrt/fork-version.mk`.
- Official OpenWrt 25.12.6 package: `2025.06.02~29236be6-r1`.
- Both have package name `nlbwmon`, so only one can be installed at a time.
- **Never** identify the fork by the upstream-looking version or by filename alone: inspect APK metadata and record SHA256.
- Package version is separate from Git commit identity. The builder records the full source commit in the adjacent `.buildinfo` file.

## Build requirements

### SDK downloads

Download **OpenWrt 25.12.6 SDKs** (not ImageBuilder, not full firmware images) from the official OpenWrt downloads site, under `releases/25.12.6/targets/`:

- `x86/64/` — `openwrt-sdk-25.12.6-x86-64_gcc-14.3.0_musl.Linux-x86_64.tar.zst` (lab x86-64 VM).
- `mediatek/filogic/` — `openwrt-sdk-25.12.6-mediatek-filogic_gcc-14.3.0_musl.Linux-x86_64.tar.zst` (MediaTek Filogic, including ASUS TUF-AX4200).

Check the exact filenames and SHA256 against the official download index and checksum file before extraction; do not assume a different release or target is interchangeable. Both SDKs are **Linux x86-64 host toolchains** and build different target architectures (`x86_64` and `aarch64_cortex-a53`). The current builder expects the extracted SDK directories under `~/projects/openwrt-sdk/` with their original directory names, plus the packages feed containing `net/nlbwmon`. Configure the SDK to build `CONFIG_PACKAGE_nlbwmon=m`, and ensure its dependencies are available. These paths are defaults of the provided local builder, not general OpenWrt requirements.

Official download root: https://downloads.openwrt.org/releases/25.12.6/targets/

Use an OpenWrt 25.12.6 SDK with the `packages` feed installed and `CONFIG_PACKAGE_nlbwmon=m` enabled. Supported local SDK targets are `x86-64` and `mediatek-filogic`. The provided script uses the existing local SDK layout under `~/projects/openwrt-sdk`; adjust the script for other layouts. The source checkout must be on `feature/flow-lifecycle-events` and have no tracked uncommitted changes.

```sh
git pull --ff-only
bash build-nlbwmon.sh --target x86-64
bash build-nlbwmon.sh --target filogic
```

The script archives committed Git source, stages it in `~/projects/nlbwmon-build-src`, preserves the original SDK feed Makefile in `Makefile.upstream.bak`, writes the fork version into the SDK recipe, and copies the fork's init script and configuration template into the SDK package files. It cleans and recompiles `nlbwmon`, verifies the output filename and SDK APK metadata, and copies the result plus SHA256 and build provenance to `~/projects/openwrt-packages/25.12.6/<target>/`. No router is modified by building.

The SDK feed's `nlbwmon.hotplug` remains from the official feed. The fork config's `flow_export` section is disabled by default. The third-party Prometheus Lua collector is **not** bundled in the APK.

## Interactive deploy and rollback (verified in the x86/64 lab)

The build script only creates packages; it does not install them. Configure `deploy-nlbwmon.local.conf` from `deploy-nlbwmon.conf.example` and **do not commit the private local configuration**. Run `bash deploy-nlbwmon.sh` for the interactive menu; review the exact SSH host, target and package before confirming `DEPLOY` or `ROLLBACK`.

The helper checks OpenWrt 25.12.6 identity and architecture, backs up router data and configuration locally, verifies the uploaded APK checksum, installs the chosen package, restarts `nlbwmon`, validates `nlbw -c csv`, and compares the configuration checksum. For rollback it discovers the matching official repository package automatically, checks its metadata, and fetches it before stopping the service. `apk update` refreshes repository indexes. The script **does not automatically recover from a failed install**, and its live database backup may be inconsistent.

### Observed lab results — 2026-11-18

- Both OpenWrt SDK builds succeeded: x86/64 (`x86_64`) and mediatek/filogic (`aarch64_cortex-a53`); both APK SHA256 files verified.
- On the **x86/64 test router**, alpha `nlbwmon-0.1.0_alpha1-r1` deployed successfully; config checksum remained unchanged; service ran and OpenSearch flow documents and Prometheus metrics continued arriving.
- Rollback to the official `nlbwmon-2025.06.02~29236be6-r1` succeeded using automatic candidate selection. `ROLLBACK OK` confirmed service running and config unchanged.
- Re-deploying alpha subsequently restored flow events according to the owner. The VM had a successful Proxmox Backup Server backup before tests.

**Scope:** these observations validate the tested x86/64 workflow, not a physical Filogic install or rollback, long-running stability, event completeness, recovery from every failure, or restoration from the PBS backup. Do not install on a production router solely based on this result.

Before any upgrade, back up the live accounting database and UCI configuration; do not replace current accounting data with an older snapshot during normal rollback. The official init script does not consume the fork's optional `flow_export` section; preserving the section allows re-deploying alpha later.

Do not publish private infrastructure addresses, SSH destinations, secrets, or backed-up configurations.

## Release gate

Only after successful build verification for each architecture and an end-to-end lab install/rollback should the project create an immutable annotated Git tag and a GitHub **Pre-release** with target-specific APKs, checksums, provenance, tested rollback instructions and known limitations.
