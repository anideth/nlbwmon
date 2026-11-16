# Keeping this fork up to date with upstream nlbwmon

This guide is for maintainers who are new to Git. It covers **source-code synchronization only**. It does not upgrade OpenWrt, install APKs, modify the router or restart services.

**Upstream** = the original [jow-/nlbwmon](https://github.com/jow-/nlbwmon), branch `master`. **Origin** = our fork [anideth/nlbwmon](https://github.com/anideth/nlbwmon). **Integration branch** = an isolated place to try an upstream update. **Release branch** = the source we intend to package after testing.

> Current beta preparation uses `release/beta-preparation`. Do not automatically merge upstream into the release or feature branch. The commands below assume a local clone in `~/projects/nlbwmon`, a terminal with Git and a clean working tree.

## 1. First-time setup (once per clone)

Inspect remotes first; do not add `upstream` twice:

```sh
cd ~/projects/nlbwmon
git remote -v
git status --short
git branch --show-current
```

If `upstream` is **absent**, add it:

```sh
git remote add upstream https://github.com/jow-/nlbwmon.git
```

If `upstream` already exists, verify its URL with `git remote -v` instead of adding it again. If `origin` is not `anideth/nlbwmon`, stop and investigate before proceeding.

## 2. Check for new upstream commits (safe, read-only)

```sh
cd ~/projects/nlbwmon
git fetch upstream
git log --oneline -10 upstream/master
git log --oneline --left-right --cherry-pick release/beta-preparation...upstream/master
git diff --stat release/beta-preparation upstream/master
```

`git fetch` downloads remote history **without merging or changing your working files**. The `<`/`>` markers in the log distinguish commits on each side; this is an overview, not a compatibility verdict. The diff may contain our existing fork changes, so inspect it before deciding whether an update is useful.

## 3. Create a disposable integration branch

Only continue if `git status --short` shows **no changes**. The following creates a backup pointer and a **new** test branch, without changing the release branch:

```sh
cd ~/projects/nlbwmon
git fetch origin
git switch release/beta-preparation
git pull --ff-only origin release/beta-preparation
git status --short
git branch backup/pre-upstream-$(date +%Y%m%d) release/beta-preparation
git switch -c integration/upstream-update release/beta-preparation
git merge --no-ff --no-commit upstream/master
```

The backup date and `integration/upstream-update` branch names must not already exist. If a command reports an error, **stop**, do not skip it. `--no-commit` lets us inspect the merge before creating a commit. No remote branches are pushed by these commands.

### If Git reports conflicts

**Do not select "accept theirs" for everything.** In particular, inspect `conntrack`, netlink, accounting, CMake and OpenWrt package files carefully. Collect diagnostics:

```sh
git status
git diff --name-only --diff-filter=U
git diff --check
```

If you are unsure, cancel the in-progress merge and return to the release branch:

```sh
git merge --abort
git switch release/beta-preparation
```

`git merge --abort` works while a merge is in progress; do not run it after a completed merge. **Never use `git reset --hard`, force-push or delete release branches as a shortcut.** If a conflict requires code changes, request review of the exact files and intended upstream behavior before proceeding.

### If Git reports no conflicts

A clean merge is **not** proof of compatibility. First inspect the result:

```sh
git diff --cached --stat
git diff --check
git status
```

For a merge that is ready to test, complete it with:

```sh
git commit -m "merge: sync upstream nlbwmon master for integration testing"
```

If Git says "Already up to date", there is no merge to commit; stop here and review whether an update is necessary. If you decide **not** to proceed while the merge is still in progress, use `git merge --abort`.

## 4. Build and validate in the lab

The existing `build-nlbwmon.sh` only permits explicit branch names (`feature/flow-lifecycle-events`, `release/beta-preparation`, `integration/upstream-update`). The integration branch created in section 3 uses the allowed name. This restriction is deliberate: the builder archives committed source and refuses dirty trees.

On the integration branch after a successful commit:

```sh
bash build-nlbwmon.sh --target x86-64
```

Do **not** deploy to the production router. Verify APK identity and SHA256, then test installation and rollback on the OpenWrt 25.12.6 x86-64 lab VM, preferably after a Proxmox snapshot. Verify:

1. Existing `nlbwmon` accounting, database and Lua/Prometheus exporter are unchanged.
2. NEW creates `active`; DESTROY updates the same OpenSearch document to `closed` with final totals. No intermediate UPDATE snapshots are emitted.
3. Conntrack mark/mask filtering, NAT attribution, TCP reconnect and queue pressure.
4. Netlink overrun recovery (socket remains registered, subsequent events continue); **lost events are not recovered**.
5. CPU/RAM, event-loss indicators, collector SQLite queue, OpenSearch indexing and rollback to the official package.
6. Both target SDK builds (x86-64 and MediaTek Filogic) before publishing a router APK.

See [BUILD_AND_ROLLBACK.md](BUILD_AND_ROLLBACK.md), [FLOW_EXPORT.md](FLOW_EXPORT.md) and the [end-to-end collector guide](https://github.com/anideth/nlbwmon-aggregator/blob/release/beta-preparation/docs/INSTALLATION_END_TO_END.md).

## 5. Approve and incorporate the result

Only after review and lab tests: open a pull request from the integration branch into the **appropriate development branch**, review the diff and test results, and merge deliberately. Promote tested changes to a release branch in a separate reviewed step. Do not auto-merge, auto-deploy or publish an APK solely because Git reported no conflicts.

## 6. Quick rollback of the Git experiment

If the integration branch has a completed commit, you can simply return to the untouched release branch:

```sh
git switch release/beta-preparation
```

This changes your **local source checkout**, not any installed router package. Keep the integration branch for debugging; delete it only after verifying it is no longer needed. Router rollback is a separate operation described in [BUILD_AND_ROLLBACK.md](BUILD_AND_ROLLBACK.md).

## 7. When should this be repeated?

Check upstream before each planned fork release and when upstream publishes security or correctness fixes. A release in upstream does **not** automatically mean our fork is compatible with it. Record the upstream commit, our merge commit, target SDK, test results and resulting APK checksum in release notes.

**Important:** the OpenWrt feed package may update separately from the upstream Git repository. A newer feed recipe or SDK can change build flags, dependencies or packaging; validate those separately and do not overwrite our fork's init/config files with an unreviewed upstream package update.
