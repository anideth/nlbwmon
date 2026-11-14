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

## Installation and rollback: not yet validated

**Do not install on the production router until the x86 lab has completed a real install and rollback test.** The build script is not an installer.

Before installation, inspect the target's OpenWrt version, target and architecture, confirm official `nlbwmon` availability in its configured APK repositories, and back up the live `/etc/config/nlbwmon`, database directory and installed package metadata. Package manager handling of configuration files must be tested: including a config template in the APK does not guarantee that a new `flow_export` section appears in an existing user configuration. Do not overwrite user settings.

A rollback must explicitly reinstall the **official repository** build (not the local alpha APK), restore the official init script, and preserve the live accounting database. The official init script ignores the extra `flow_export` section; keep that section for possible later reinstall, but verify actual behavior in the lab. Test the exact downgrade command and repository selection before documenting it as working. A month-old database backup must not replace the live database during ordinary rollback.

Do not publish private hostnames, IP addresses, TCP collector endpoints or backed-up configuration in public releases.

## Release gate

Only after successful build verification for each architecture and an end-to-end lab install/rollback should the project create an immutable annotated Git tag and a GitHub **Pre-release** with target-specific APKs, checksums, provenance, tested rollback instructions and known limitations.
