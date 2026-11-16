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

Use an OpenWrt 25.12.6 SDK with the `packages` feed installed and `CONFIG_PACKAGE_nlbwmon=m` enabled. Supported local SDK targets are `x86-64` and `mediatek-filogic`. The provided script uses the existing local SDK layout under `~/projects/openwrt-sdk`; adjust the script for other layouts. The source checkout must be on one of the explicitly permitted branches (`feature/flow-lifecycle-events`, `release/beta-preparation`, `integration/upstream-update`) and have no tracked uncommitted changes. Follow [UPSTREAM_SYNC.md](UPSTREAM_SYNC.md) for safe integration of updates from the original project.

```sh
git pull --ff-only
bash build-nlbwmon.sh --target x86-64
bash build-nlbwmon.sh --target filogic
```

The adjacent `.sha256` file should contain only the APK basename, so it can be checked after downloading the APK and checksum into the same directory using `sha256sum -c <checksum-file>`. Rebuild both targets after changes to the build script to ensure the `.buildinfo` Git commit matches the released source.

The script archives committed Git source, stages it in `~/projects/nlbwmon-build-src`, preserves the original SDK feed Makefile in `Makefile.upstream.bak`, writes the fork version into the SDK recipe, and copies the fork's init script and configuration template into the SDK package files. It cleans and recompiles `nlbwmon`, verifies the output filename and SDK APK metadata, and copies the result plus SHA256 and build provenance to `~/projects/openwrt-packages/25.12.6/<target>/`. No router is modified by building.

The SDK feed's `nlbwmon.hotplug` remains from the official feed. The fork config's `flow_export` section is disabled by default. The third-party Prometheus Lua collector is **not** bundled in the APK.

## Interactive deploy and rollback (lab-tested by maintainer)

On 2026-11-16, the maintainer reinstalled the x86-64 `0.1.0_alpha1-r1` APK built from commit `07239d1f954fba86e187f9e15355e37c9542f2e4` on an OpenWrt 25.12.6 lab VM. The script reported a successful install, preserved UCI configuration, and restarted the daemon. Metrics and NEW/DESTROY document updates were observed in OpenSearch. This test did not revalidate the official-package downgrade path for this exact candidate or install the matching Filogic APK on hardware.

**The maintainer reports that interactive lab installation and rollback were exercised; this does not establish universal device/firmware compatibility or production safety.** The build script is not an installer.

Copy `deploy-nlbwmon.conf.example` to `deploy-nlbwmon.local.conf`, edit your SSH destinations and keep the local file out of Git (it is ignored). Run `bash deploy-nlbwmon.sh` for the interactive menu. The script provides read-only check, alpha deploy and explicit official-repository rollback. It checks the remote OpenWrt release, target and architecture, makes a local backup of the router's current config/database, checks SHA256 on uploaded alpha packages, stops and restarts the service and verifies that the config checksum did not change. Rollback prompts for the exact official package version and attempts to fetch that package from the device's configured APK repositories **before** stopping the service. The maintainer reports lab installation and rollback tests. Revalidate `apk fetch`, downgrade flags and UCI preservation on the exact target firmware before any production deployment.

**Safety:** The script does not automatically restore a backup or roll back a failed install. Use a Proxmox snapshot of the lab VM when repeating the test, and retain an independent recovery path. The private local config must never be included in the release.\n

Before installation, inspect the target's OpenWrt version, target and architecture, confirm official `nlbwmon` availability in its configured APK repositories, and back up the live `/etc/config/nlbwmon`, database directory and installed package metadata. Package manager handling of configuration files must be tested: including a config template in the APK does not guarantee that a new `flow_export` section appears in an existing user configuration. Do not overwrite user settings.

A rollback must explicitly reinstall the **official repository** build (not the local alpha APK), restore the official init script, and preserve the live accounting database. The official init script ignores the extra `flow_export` section; keep that section for possible later reinstall, but verify actual behavior in the lab. Test the exact downgrade command and repository selection before documenting it as working. A month-old database backup must not replace the live database during ordinary rollback.

Do not publish private hostnames, IP addresses, TCP collector endpoints or backed-up configuration in public releases.

## Release gate

Only after successful build verification for each architecture and an end-to-end lab install/rollback should the project create an immutable annotated Git tag and a GitHub **Pre-release** with target-specific APKs, checksums, provenance, tested rollback instructions and known limitations.
